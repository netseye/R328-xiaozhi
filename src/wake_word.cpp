#include "wake_word.h"

#ifdef XZ_TARGET_DEVICE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* TFLM headers */
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/micro/micro_resource_variable.h"
#include "tensorflow/lite/micro/micro_allocator.h"
#include "tensorflow/lite/micro/system_setup.h"
#include "tensorflow/lite/schema/schema_generated.h"

/* TFLM audio frontend (exact same as used during model training) */
#include "tensorflow/lite/experimental/microfrontend/lib/frontend.h"
#include "tensorflow/lite/experimental/microfrontend/lib/frontend_util.h"

/* Feature ring buffer: model input is [1, 3, 40], so we buffer 3 spectrogram frames */
#define FEATURE_HISTORY     3
#define NUM_CHANNELS        40

struct xz_wake_t {
    /* TFLM model data */
    uint8_t *model_data;
    int model_size;

    /* TFLM interpreter */
    tflite::MicroInterpreter *interpreter;
    TfLiteTensor *input;
    TfLiteTensor *output;

    /* Tensor arena */
    uint8_t *tensor_arena;
    int arena_size;

    /* Resource variables for stateful streaming model */
    uint8_t *var_arena;
    int var_arena_size;
    tflite::MicroResourceVariables *resource_vars;

    /* Op resolver (lifetime must exceed interpreter) */
    tflite::MicroMutableOpResolver<20> *resolver;

    /* TFLM audio frontend */
    struct FrontendState frontend_state;
    int frontend_initialized;

    /* Feature ring buffer: [FEATURE_HISTORY * NUM_CHANNELS] */
    int8_t *features;
    int feature_count;

    /* Detection state */
    float probability_cutoff;
    int sliding_window_size;
    int consecutive_detections;
};

static uint8_t *load_model_file(const char *path, int *size) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (len <= 0) { fclose(f); return NULL; }
    uint8_t *buf = (uint8_t *)malloc(len);
    if (!buf) { fclose(f); return NULL; }
    *size = (int)fread(buf, 1, len, f);
    fclose(f);
    return buf;
}

/* --- Public API --- */

xz_wake_t *xz_wake_create(const xz_wake_config_t *cfg) {
    if (cfg->sample_rate != 16000) {
        fprintf(stderr, "[WAKE] Only 16kHz sample rate supported\n");
        return NULL;
    }

    xz_wake_t *w = (xz_wake_t *)calloc(1, sizeof(*w));
    if (!w) return NULL;

    w->probability_cutoff = cfg->probability_cutoff > 0 ? cfg->probability_cutoff : 0.97f;
    w->sliding_window_size = cfg->sliding_window_size > 0 ? cfg->sliding_window_size : 5;

    /* Load model */
    w->model_data = load_model_file(cfg->model_path, &w->model_size);
    if (!w->model_data) {
        fprintf(stderr, "[WAKE] Failed to load model: %s\n", cfg->model_path);
        free(w);
        return NULL;
    }
    printf("[WAKE] Model loaded: %d bytes\n", w->model_size);

    /* Initialize TFLM audio frontend (same pipeline used during training) */
    struct FrontendConfig frontend_config;
    FrontendFillConfigWithDefaults(&frontend_config);
    frontend_config.window.size_ms = 30;
    frontend_config.window.step_size_ms = cfg->feature_step_ms > 0 ? cfg->feature_step_ms : 10;
    frontend_config.filterbank.num_channels = NUM_CHANNELS;
    frontend_config.filterbank.lower_band_limit = 125.0f;
    frontend_config.filterbank.upper_band_limit = 7500.0f;
    frontend_config.noise_reduction.smoothing_bits = 10;
    frontend_config.noise_reduction.even_smoothing = 0.025f;
    frontend_config.noise_reduction.odd_smoothing = 0.06f;
    frontend_config.noise_reduction.min_signal_remaining = 0.05f;
    frontend_config.pcan_gain_control.enable_pcan = 1;
    frontend_config.pcan_gain_control.strength = 0.95f;
    frontend_config.pcan_gain_control.offset = 80.0f;
    frontend_config.pcan_gain_control.gain_bits = 21;
    frontend_config.log_scale.enable_log = 1;
    frontend_config.log_scale.scale_shift = 6;

    if (FrontendPopulateState(&frontend_config, &w->frontend_state, cfg->sample_rate) != 1) {
        fprintf(stderr, "[WAKE] FrontendPopulateState failed (malloc issue?)\n");
        /* Don't destroy - frontend is partially initialized, just disable */
        w->frontend_initialized = 0;
        xz_wake_destroy(w);
        return NULL;
    }
    w->frontend_initialized = 1;
    printf("[WAKE] Audio frontend initialized: window=%dms, step=%dms, channels=%d\n",
           frontend_config.window.size_ms, frontend_config.window.step_size_ms,
           frontend_config.filterbank.num_channels);

    /* Feature ring buffer */
    w->features = (int8_t *)calloc(FEATURE_HISTORY * NUM_CHANNELS, sizeof(int8_t));
    if (!w->features) {
        fprintf(stderr, "[WAKE] Feature allocation failed\n");
        xz_wake_destroy(w);
        return NULL;
    }

    /* Set up TFLM interpreter */
    const tflite::Model *model = tflite::GetModel(w->model_data);
    if (!model) {
        fprintf(stderr, "[WAKE] Invalid TFLite model\n");
        xz_wake_destroy(w);
        return NULL;
    }

    /* Tensor arena for activations */
    w->arena_size = 65536;
    w->tensor_arena = (uint8_t *)malloc(w->arena_size);
    if (!w->tensor_arena) {
        fprintf(stderr, "[WAKE] Arena allocation failed\n");
        xz_wake_destroy(w);
        return NULL;
    }

    /* Variable arena for resource variables (stateful streaming model) */
    w->var_arena_size = 4096;
    w->var_arena = (uint8_t *)malloc(w->var_arena_size);
    if (!w->var_arena) {
        fprintf(stderr, "[WAKE] Variable arena allocation failed\n");
        xz_wake_destroy(w);
        return NULL;
    }

    /* Create MicroResourceVariables for streaming state management */
    tflite::MicroAllocator *var_alloc = tflite::MicroAllocator::Create(
        w->var_arena, w->var_arena_size);
    if (!var_alloc) {
        fprintf(stderr, "[WAKE] MicroAllocator creation failed\n");
        xz_wake_destroy(w);
        return NULL;
    }
    w->resource_vars = tflite::MicroResourceVariables::Create(var_alloc, 10);
    if (!w->resource_vars) {
        fprintf(stderr, "[WAKE] MicroResourceVariables creation failed\n");
        xz_wake_destroy(w);
        return NULL;
    }

    /* Op resolver - matching esphome's register_streaming_ops_ */
    w->resolver = new tflite::MicroMutableOpResolver<20>();
    w->resolver->AddCallOnce();
    w->resolver->AddVarHandle();
    w->resolver->AddReshape();
    w->resolver->AddReadVariable();
    w->resolver->AddStridedSlice();
    w->resolver->AddConcatenation();
    w->resolver->AddAssignVariable();
    w->resolver->AddConv2D();
    w->resolver->AddMul();
    w->resolver->AddAdd();
    w->resolver->AddMean();
    w->resolver->AddFullyConnected();
    w->resolver->AddLogistic();
    w->resolver->AddQuantize();
    w->resolver->AddDepthwiseConv2D();
    w->resolver->AddAveragePool2D();
    w->resolver->AddMaxPool2D();
    w->resolver->AddPad();
    w->resolver->AddPack();
    w->resolver->AddSplitV();

    w->interpreter = new tflite::MicroInterpreter(
        model, *w->resolver, w->tensor_arena, w->arena_size, w->resource_vars);

    TfLiteStatus status = w->interpreter->AllocateTensors();
    if (status != kTfLiteOk) {
        fprintf(stderr, "[WAKE] AllocateTensors failed\n");
        xz_wake_destroy(w);
        return NULL;
    }

    w->input = w->interpreter->input(0);
    w->output = w->interpreter->output(0);

    if (!w->input || !w->input->dims) {
        fprintf(stderr, "[WAKE] Input tensor has no dims\n");
        xz_wake_destroy(w);
        return NULL;
    }
    if (w->input->dims->size != 3 || w->input->dims->data[2] != NUM_CHANNELS) {
        fprintf(stderr, "[WAKE] Unexpected input shape: [%d,%d,%d]\n",
                w->input->dims->data[0], w->input->dims->data[1],
                w->input->dims->data[2]);
        xz_wake_destroy(w);
        return NULL;
    }

    int used = w->interpreter->arena_used_bytes();
    printf("[WAKE] Input: [%d,%d,%d] %s, quant: scale=%.6f, zp=%d\n",
           w->input->dims->data[0], w->input->dims->data[1],
           w->input->dims->data[2],
           TfLiteTypeGetName(w->input->type),
           w->input->params.scale, w->input->params.zero_point);
    printf("[WAKE] Output: [%d,%d] %s\n",
           w->output->dims->data[0], w->output->dims->data[1],
           TfLiteTypeGetName(w->output->type));
    printf("[WAKE] Arena used: %d / %d bytes\n", used, w->arena_size);

    if (used >= w->arena_size) {
        fprintf(stderr, "[WAKE] Arena too small! Need at least %d bytes\n", used);
        xz_wake_destroy(w);
        return NULL;
    }

    return w;
}

void xz_wake_destroy(xz_wake_t *w) {
    if (!w) return;
    if (w->interpreter) delete w->interpreter;
    delete w->resolver;
    if (w->frontend_initialized)
        FrontendFreeStateContents(&w->frontend_state);
    free(w->tensor_arena);
    free(w->var_arena);
    free(w->model_data);
    free(w->features);
    free(w);
}

void xz_wake_reset(xz_wake_t *w) {
    if (!w) return;
    w->feature_count = 0;
    w->consecutive_detections = 0;
    memset(w->features, 0, FEATURE_HISTORY * NUM_CHANNELS);
    if (w->frontend_initialized)
        FrontendReset(&w->frontend_state);
}

int xz_wake_process(xz_wake_t *w, const int16_t *samples, int count) {
    if (!w || !w->interpreter) return -1;

    /* Apply software gain to compensate for low mic sensitivity */
    int16_t gain_buf[160];
    if (count > 160) count = 160;
    for (int i = 0; i < count; i++) {
        int32_t s = (int32_t)samples[i] * 16;
        if (s > 32767) s = 32767;
        if (s < -32768) s = -32768;
        gain_buf[i] = (int16_t)s;
    }

    /* Feed samples through TFLM microfrontend */
    size_t num_samples_read = 0;
    struct FrontendOutput output = FrontendProcessSamples(
        &w->frontend_state, gain_buf, count, &num_samples_read);

    /* Frontend returns a new feature frame only when enough samples accumulated */
    if (output.size == 0)
        return 0;

    /* Quantize uint16 frontend output to INT8 using esphome's fixed formula.
     * This matches the training pipeline exactly. */
    /* Shift feature history left */
    memmove(w->features, w->features + NUM_CHANNELS,
            (FEATURE_HISTORY - 1) * NUM_CHANNELS);

    /* Log raw frontend output periodically (~2s) */
    static int fe_dbg = 0;
    fe_dbg++;
    if (fe_dbg % 100 == 0) {
        uint32_t fe_sum = 0;
        for (int i = 0; i < (int)output.size; i++) fe_sum += output.values[i];
        fprintf(stderr, "[WAKE-FE] avg=%u\n", (unsigned)(fe_sum / output.size));
    }

    /* Add new frame at the end */
    for (int i = 0; i < (int)output.size && i < NUM_CHANNELS; i++) {
        int32_t value = ((output.values[i] * 256) + 333) / 666;
        value += (-128);  /* INT8_MIN */
        if (value < -128) value = -128;
        if (value > 127) value = 127;
        w->features[(FEATURE_HISTORY - 1) * NUM_CHANNELS + i] = (int8_t)value;
    }

    if (w->feature_count < FEATURE_HISTORY) {
        w->feature_count++;
        return 0;
    }

    /* Copy features to model input tensor */
    int8_t *input_data = w->input->data.int8;
    memcpy(input_data, w->features, FEATURE_HISTORY * NUM_CHANNELS);

    /* Run inference */
    if (w->interpreter->Invoke() != kTfLiteOk) {
        return -1;
    }

    /* Read output probability */
    float probability = 0.0f;
    if (w->output->type == kTfLiteUInt8) {
        float out_scale = w->output->params.scale;
        int out_zp = w->output->params.zero_point;
        probability = (float)(w->output->data.uint8[0] - out_zp) * out_scale;
    } else if (w->output->type == kTfLiteInt8) {
        float out_scale = w->output->params.scale;
        int out_zp = w->output->params.zero_point;
        probability = (float)(w->output->data.int8[0] - out_zp) * out_scale;
    }

    /* Debug: log non-zero probability or periodically */
    static int dbg_count = 0;
    dbg_count++;
    int raw_out = w->output->type == kTfLiteUInt8 ? w->output->data.uint8[0] : w->output->data.int8[0];
    if (raw_out > 0) {
        fprintf(stderr, "[WAKE] prob=%.4f raw=%d feat=[%d,%d,%d,%d,%d,..%d,%d,%d,%d,%d]\n",
                probability, raw_out,
                w->features[0], w->features[1], w->features[2],
                w->features[3], w->features[4],
                w->features[76], w->features[77], w->features[78],
                w->features[79], w->features[80]);
    }

    /* Sliding window detection */
    if (probability >= w->probability_cutoff) {
        w->consecutive_detections++;
        if (w->consecutive_detections >= w->sliding_window_size) {
            w->consecutive_detections = 0;
            printf("[WAKE] Detected! probability=%.3f\n", probability);
            return 1;
        }
    } else {
        w->consecutive_detections = 0;
    }

    return 0;
}

#else /* Host stubs */

#include <stdio.h>
#include <stdlib.h>

struct xz_wake_t { int dummy; };

xz_wake_t *xz_wake_create(const xz_wake_config_t *cfg) {
    (void)cfg;
    printf("[WAKE] Skipped (host build)\n");
    return static_cast<xz_wake_t *>(calloc(1, sizeof(xz_wake_t)));
}
void xz_wake_destroy(xz_wake_t *w) { free(w); }
int xz_wake_process(xz_wake_t *w, const int16_t *samples, int count) {
    (void)w; (void)samples; (void)count; return 0;
}
void xz_wake_reset(xz_wake_t *w) { (void)w; }

#endif
