#include "audio.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef XZ_TARGET_DEVICE
#include <alsa/asoundlib.h>
#include <opus/opus.h>
#include <opus/opus_types.h>

struct xz_audio_t {
    snd_pcm_t *capture_pcm;
    snd_pcm_t *playback_pcm;
    OpusEncoder *encoder;
    OpusDecoder *decoder;
    int16_t *capture_buf;
    int16_t *playback_buf;
    int frame_samples;
    int playback_frame_samples;
    int capturing;
    int playing;
};

xz_audio_t *xz_audio_create(const xz_audio_config_t *cfg) {
    xz_audio_t *a = calloc(1, sizeof(*a));
    if (!a) return NULL;

    a->frame_samples = cfg->sample_rate * cfg->frame_ms / 1000;
    a->playback_frame_samples = cfg->output_sample_rate * cfg->frame_ms / 1000;
    a->capture_buf = malloc(a->frame_samples * sizeof(int16_t));
    a->playback_buf = malloc(a->playback_frame_samples * sizeof(int16_t));
    if (!a->capture_buf || !a->playback_buf) {
        free(a->capture_buf); free(a->playback_buf); free(a); return NULL;
    }

    int err;
    a->encoder = opus_encoder_create(cfg->sample_rate, cfg->channels,
                                      OPUS_APPLICATION_VOIP, &err);
    if (err != OPUS_OK) {
        fprintf(stderr, "opus_encoder_create failed: %s\n", opus_strerror(err));
        free(a->capture_buf); free(a->playback_buf); free(a); return NULL;
    }

    a->decoder = opus_decoder_create(cfg->output_sample_rate, cfg->channels, &err);
    if (err != OPUS_OK) {
        fprintf(stderr, "opus_decoder_create failed: %s\n", opus_strerror(err));
        opus_encoder_destroy(a->encoder);
        free(a->capture_buf); free(a->playback_buf); free(a); return NULL;
    }

    err = snd_pcm_open(&a->capture_pcm, cfg->capture_dev,
                       SND_PCM_STREAM_CAPTURE, 0);
    if (err < 0) {
        fprintf(stderr, "snd_pcm_open capture failed: %s\n", snd_strerror(err));
        goto fail;
    }

    snd_pcm_hw_params_t *params;
    snd_pcm_hw_params_alloca(&params);
    snd_pcm_hw_params_any(a->capture_pcm, params);
    snd_pcm_hw_params_set_access(a->capture_pcm, params, SND_PCM_ACCESS_RW_INTERLEAVED);
    snd_pcm_hw_params_set_format(a->capture_pcm, params, SND_PCM_FORMAT_S16_LE);
    snd_pcm_hw_params_set_channels(a->capture_pcm, params, cfg->channels);
    unsigned int rate = cfg->sample_rate;
    snd_pcm_hw_params_set_rate_near(a->capture_pcm, params, &rate, 0);
    snd_pcm_hw_params_set_period_size(a->capture_pcm, params, a->frame_samples, 0);
    err = snd_pcm_hw_params(a->capture_pcm, params);
    if (err < 0) {
        fprintf(stderr, "snd_pcm_hw_params capture failed: %s\n", snd_strerror(err));
        goto fail;
    }

    err = snd_pcm_open(&a->playback_pcm, cfg->playback_dev,
                       SND_PCM_STREAM_PLAYBACK, 0);
    if (err < 0) {
        fprintf(stderr, "snd_pcm_open playback failed: %s\n", snd_strerror(err));
        goto fail;
    }

    snd_pcm_hw_params_alloca(&params);
    snd_pcm_hw_params_any(a->playback_pcm, params);
    snd_pcm_hw_params_set_access(a->playback_pcm, params, SND_PCM_ACCESS_RW_INTERLEAVED);
    snd_pcm_hw_params_set_format(a->playback_pcm, params, SND_PCM_FORMAT_S16_LE);
    snd_pcm_hw_params_set_channels(a->playback_pcm, params, cfg->channels);
    rate = cfg->output_sample_rate;
    snd_pcm_hw_params_set_rate_near(a->playback_pcm, params, &rate, 0);
    snd_pcm_hw_params_set_period_size(a->playback_pcm, params, a->playback_frame_samples, 0);
    err = snd_pcm_hw_params(a->playback_pcm, params);
    if (err < 0) {
        fprintf(stderr, "snd_pcm_hw_params playback failed: %s\n", snd_strerror(err));
        goto fail;
    }

    system("amixer cset numid=8 on");    /* Spk PA Switch */
    system("amixer cset numid=10 on");   /* Lineout Switch */
    system("amixer cset numid=2 63");    /* digital volume max */

    return a;

fail:
    if (a->capture_pcm) snd_pcm_close(a->capture_pcm);
    if (a->playback_pcm) snd_pcm_close(a->playback_pcm);
    opus_encoder_destroy(a->encoder);
    opus_decoder_destroy(a->decoder);
    free(a->capture_buf);
    free(a->playback_buf);
    free(a);
    return NULL;
}

void xz_audio_destroy(xz_audio_t *a) {
    if (!a) return;
    xz_audio_stop_capture(a);
    xz_audio_stop_playback(a);
    if (a->capture_pcm) snd_pcm_close(a->capture_pcm);
    if (a->playback_pcm) snd_pcm_close(a->playback_pcm);
    opus_encoder_destroy(a->encoder);
    opus_decoder_destroy(a->decoder);
    free(a->capture_buf);
    free(a->playback_buf);
    free(a);
}

int xz_audio_start_capture(xz_audio_t *a) {
    if (!a || a->capturing) return -1;
    snd_pcm_prepare(a->capture_pcm);
    a->capturing = 1;
    return 0;
}

void xz_audio_stop_capture(xz_audio_t *a) {
    if (a && a->capturing) {
        a->capturing = 0;
        snd_pcm_drop(a->capture_pcm);
    }
}

int xz_audio_read_opus(xz_audio_t *a, uint8_t *buf, int bufsize) {
    if (!a || !a->capturing) return -1;
    int frames = snd_pcm_readi(a->capture_pcm, a->capture_buf, a->frame_samples);
    if (frames < 0) {
        frames = snd_pcm_recover(a->capture_pcm, frames, 0);
        if (frames < 0) return -1;
    }
    int nbytes = opus_encode(a->encoder, a->capture_buf, frames, buf, bufsize);
    return (nbytes < 0) ? -1 : nbytes;
}

int xz_audio_read_pcm(xz_audio_t *a, int16_t *buf, int max_samples) {
    if (!a || !a->capturing) return -1;
    int to_read = max_samples < a->frame_samples ? max_samples : a->frame_samples;
    int frames = snd_pcm_readi(a->capture_pcm, buf, to_read);
    if (frames < 0) {
        frames = snd_pcm_recover(a->capture_pcm, frames, 0);
        if (frames < 0) return -1;
    }
    return frames;
}

int xz_audio_start_playback(xz_audio_t *a) {
    if (!a || a->playing) return -1;
    snd_pcm_prepare(a->playback_pcm);
    a->playing = 1;
    return 0;
}

void xz_audio_stop_playback(xz_audio_t *a) {
    if (a && a->playing) {
        a->playing = 0;
        snd_pcm_drop(a->playback_pcm);
    }
}

int xz_audio_play_opus(xz_audio_t *a, const uint8_t *data, int len) {
    if (!a || !a->playing) return -1;
    int frames = opus_decode(a->decoder, data, len, a->playback_buf, a->playback_frame_samples, 0);
    if (frames < 0) return -1;
    int played = snd_pcm_writei(a->playback_pcm, a->playback_buf, frames);
    if (played < 0) {
        played = snd_pcm_recover(a->playback_pcm, played, 0);
        if (played < 0) return -1;
    }
    return 0;
}

int xz_audio_set_volume(xz_audio_t *a, int volume) {
    if (!a) return -1;
    char cmd[128];
    snprintf(cmd, sizeof(cmd), "amixer cset 'LINEOUT volume' %d", volume);
    return system(cmd);
}

#else /* Host stubs */

struct xz_audio_t { int dummy; };

xz_audio_t *xz_audio_create(const xz_audio_config_t *cfg) { (void)cfg; return calloc(1, sizeof(xz_audio_t)); }
void xz_audio_destroy(xz_audio_t *a) { free(a); }
int xz_audio_start_capture(xz_audio_t *a) { (void)a; return 0; }
void xz_audio_stop_capture(xz_audio_t *a) { (void)a; }
int xz_audio_read_opus(xz_audio_t *a, uint8_t *buf, int bufsize) { (void)a; (void)buf; (void)bufsize; return 0; }
int xz_audio_read_pcm(xz_audio_t *a, int16_t *buf, int max_samples) { (void)a; (void)buf; (void)max_samples; return 0; }
int xz_audio_start_playback(xz_audio_t *a) { (void)a; return 0; }
void xz_audio_stop_playback(xz_audio_t *a) { (void)a; }
int xz_audio_play_opus(xz_audio_t *a, const uint8_t *data, int len) { (void)a; (void)data; (void)len; return 0; }
int xz_audio_set_volume(xz_audio_t *a, int volume) { (void)a; (void)volume; return 0; }

#endif
