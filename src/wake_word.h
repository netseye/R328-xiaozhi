#ifndef XZ_WAKE_WORD_H
#define XZ_WAKE_WORD_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Configuration for the wake word detector */
typedef struct {
    int sample_rate;          /* Audio sample rate (must be 16000) */
    const char *model_path;   /* Path to .tflite model file */
    float probability_cutoff; /* Detection threshold (0.0-1.0, e.g. 0.97) */
    int sliding_window_size;  /* Number of consecutive detections needed */
    int feature_step_ms;      /* Spectrogram step in ms (typically 10) */
} xz_wake_config_t;

/* Opaque detector handle */
typedef struct xz_wake_t xz_wake_t;

/* Create/destroy */
xz_wake_t *xz_wake_create(const xz_wake_config_t *cfg);
void xz_wake_destroy(xz_wake_t *w);

/* Feed PCM audio samples (16-bit mono, 16000Hz).
   Returns 1 if wake word detected, 0 otherwise, -1 on error.
   Feed 160 samples at a time (10ms at 16kHz = feature step size). */
int xz_wake_process(xz_wake_t *w, const int16_t *samples, int count);

/* Reset internal state (call after detection or when stopping) */
void xz_wake_reset(xz_wake_t *w);

#ifdef __cplusplus
}
#endif

#endif
