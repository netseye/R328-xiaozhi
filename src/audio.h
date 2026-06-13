#ifndef XZ_AUDIO_H
#define XZ_AUDIO_H

#include <stddef.h>
#include <stdint.h>

#define XZ_OPUS_FRAME_MS 60
#define XZ_SAMPLE_RATE 16000
#define XZ_CHANNELS 1
#define XZ_FRAME_SAMPLES (XZ_SAMPLE_RATE * XZ_OPUS_FRAME_MS / 1000)
#define XZ_PCM_FRAME_BYTES (XZ_FRAME_SAMPLES * XZ_CHANNELS * 2)
#define XZ_OPUS_MAX_BYTES 1276

typedef struct {
    int sample_rate;
    int output_sample_rate;
    int channels;
    int frame_ms;
    char capture_dev[64];
    char playback_dev[64];
} xz_audio_config_t;

typedef struct xz_audio_t xz_audio_t;

xz_audio_t *xz_audio_create(const xz_audio_config_t *cfg);
void xz_audio_destroy(xz_audio_t *a);
int xz_audio_start_capture(xz_audio_t *a);
void xz_audio_stop_capture(xz_audio_t *a);
int xz_audio_read_opus(xz_audio_t *a, uint8_t *buf, int bufsize);
int xz_audio_read_pcm(xz_audio_t *a, int16_t *buf, int max_samples);
int xz_audio_play_opus(xz_audio_t *a, const uint8_t *data, int len);
int xz_audio_start_playback(xz_audio_t *a);
void xz_audio_stop_playback(xz_audio_t *a);
int xz_audio_set_volume(xz_audio_t *a, int volume);

#endif
