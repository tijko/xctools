#ifndef AUDIO_HELPER_H
#define AUDIO_HELPER_H

#define ALSA_PCM_NEW_HW_PARAMS_API
#include <alsa/asoundlib.h>

#include <stdbool.h>
#include <sys/types.h>

typedef struct Settings {

    snd_pcm_t *handle;
    snd_mixer_t *mhandle;

    int32_t fmt;
    int32_t freq;
    int32_t valid;
    int32_t nchannels;
    int32_t sample_size;

} Settings;

int openxt_alsa_remove_pcm(void);
int openxt_alsa_mixer_fini(Settings *settings);
int openxt_alsa_mixer_init(Settings *settings);
int openxt_alsa_fini(Settings *settings);
int openxt_alsa_init(Settings *settings, char *name, snd_pcm_stream_t stream);
int openxt_alsa_prepare(Settings *settings);
int openxt_alsa_drop(Settings *settings);
int openxt_alsa_get_available(Settings *settings);
int openxt_alsa_writei(Settings *settings, char *buffer, int32_t num, int32_t size);
int openxt_alsa_set_playback_volume(Settings *settings, int64_t left, int64_t right);

#endif // AUDIO_HELPER_H