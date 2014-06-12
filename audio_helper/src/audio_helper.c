/*
 * audio_helper.c:
 *
 * Copyright (c) 2011 Stefano Panella <stefano.panella@citrix.com>,
 * All rights reserved.
 *
 */

/*
 * Copyright (c) 2013 Citrix Systems, Inc.
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */


#include "project.h"
#include <libv4v.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <alsa/asoundlib.h>

typedef enum {
    AUD_FMT_U8,
    AUD_FMT_S8,
    AUD_FMT_U16,
    AUD_FMT_S16,
    AUD_FMT_U32,
    AUD_FMT_S32
} audfmt_e;

static struct {
    int size_in_usec_in;
    int size_in_usec_out;
    const char *pcm_name_in;
    const char *pcm_name_out;
    unsigned int buffer_size_in;
    unsigned int period_size_in;
    unsigned int buffer_size_out;
    unsigned int period_size_out;
    unsigned int threshold;

    int buffer_size_in_overridden;
    int period_size_in_overridden;

    int buffer_size_out_overridden;
    int period_size_out_overridden;
    int verbose;

    const char *volume_control;
} conf = {
    .buffer_size_out = 1024,
    .pcm_name_out = "default",
    .pcm_name_in = "default",
    .volume_control = "Master",
};

char pcm_name_in[256];
char pcm_name_out[256];
char volume_control[256];

struct alsa_params_req {
    int freq;
    snd_pcm_format_t fmt;
    int nchannels;
    int size_in_usec;
    int override_mask;
    unsigned int buffer_size;
    unsigned int period_size;
};

struct alsa_params_obt {
    int freq;
    audfmt_e fmt;
    int endianness;
    int nchannels;
    snd_pcm_uframes_t samples;
};

#define AUDIO_DEBUG

#ifdef AUDIO_DEBUG
# define AH_LOG(Args...) printf(Args)
# define AH_HEX_DUMP(buf, len) ah_hex_dump(buf, len)
#else
# define AH_LOG(Args...)
# define AH_HEX_DUMP(buf, len)
#endif

#define V4V_TYPE 'W'
#define V4VIOCSETRINGSIZE       _IOW (V4V_TYPE,  1, uint32_t)

#define AUDIO_PORT 5001

#define V4V_AUDIO_RING_SIZE (V4V_ROUNDUP((((4096)*32) - sizeof(v4v_ring_t)-V4V_ROUNDUP(1))))

/* Messages definition */
#define AUDIO_INIT                             0x00
#define AUDIO_ALSA_OPEN                        0x01
#define AUDIO_VOLUME                           0x02
#define AUDIO_VOLUME_MONO                      0x03

#define AUDIO_SND_PCM_CLOSE                    0x04
#define AUDIO_SND_PCM_PREPARE                  0x05
#define AUDIO_SND_PCM_DROP                     0x06
#define AUDIO_SND_PCM_AVAIL_UPDATE             0x07
#define AUDIO_SND_PCM_STATE                    0x08
#define AUDIO_SND_PCM_WRITEI                   0x09
#define AUDIO_SND_PCM_READI                    0x10
#define AUDIO_SND_PCM_RESUME                   0x11
#define AUDIO_VOL_CTRL_REMOVE                  0x12

#define MAX_V4V_MSG_SIZE (V4V_AUDIO_RING_SIZE)

struct audio_helper {
    int fd;
    v4v_addr_t remote_addr;
    v4v_addr_t local_addr;
    uint8_t io_buf[MAX_V4V_MSG_SIZE];
    int stubdom_id;
};

static void ah_hex_dump(const void* address, uint32_t len)
{
    const unsigned char* p = address;
    int i, j;

    for (i = 0; i < len; i += 16) {
	for (j = 0; j < 16 && i + j < len; j++)
	    AH_LOG("%02x ", p[i + j]);
	for (; j < 16; j++)
	    AH_LOG("   ");
	AH_LOG(" ");
	for (j = 0; j < 16 && i + j < len; j++)
	    AH_LOG("%c", (p[i + j] < ' ' || p[i + j] > 0x7e) ? '.' : p[i + j]);
	AH_LOG("\n");
    }
}

static int alsa_to_audfmt (snd_pcm_format_t alsafmt, audfmt_e *fmt,
                           int *endianness)
{
    AH_LOG("%s\n", __FUNCTION__);
    switch (alsafmt) {
    case SND_PCM_FORMAT_S8:
        *endianness = 0;
        *fmt = AUD_FMT_S8;
        break;

    case SND_PCM_FORMAT_U8:
        *endianness = 0;
        *fmt = AUD_FMT_U8;
        break;

    case SND_PCM_FORMAT_S16_LE:
        *endianness = 0;
        *fmt = AUD_FMT_S16;
        break;

    case SND_PCM_FORMAT_U16_LE:
        *endianness = 0;
        *fmt = AUD_FMT_U16;
        break;

    case SND_PCM_FORMAT_S16_BE:
        *endianness = 1;
        *fmt = AUD_FMT_S16;
        break;

    case SND_PCM_FORMAT_U16_BE:
        *endianness = 1;
        *fmt = AUD_FMT_U16;
        break;

    case SND_PCM_FORMAT_S32_LE:
        *endianness = 0;
        *fmt = AUD_FMT_S32;
        break;

    case SND_PCM_FORMAT_U32_LE:
        *endianness = 0;
        *fmt = AUD_FMT_U32;
        break;

    case SND_PCM_FORMAT_S32_BE:
        *endianness = 1;
        *fmt = AUD_FMT_S32;
        break;

    case SND_PCM_FORMAT_U32_BE:
        *endianness = 1;
        *fmt = AUD_FMT_U32;
        break;

    default:
        AH_LOG("Unrecognized audio format %d\n", alsafmt);
        return -1;
    }

    return 0;
}

void alsa_volume(int rvol, int lvol, int mute)
{
    static snd_mixer_t *handle = NULL;
    static const char *card = "default";
    snd_mixer_elem_t *elem;
    snd_mixer_selem_id_t *sid;
    int err, chn, volume;

    AH_LOG("%s rvol=%d, lvol=%d, mute=%d\n",
	   __FUNCTION__, rvol, lvol, mute);
    snd_mixer_selem_id_alloca(&sid);

    if (handle == NULL) {
	if ((err = snd_mixer_open(&handle, 0)) < 0) {
	    AH_LOG("Mixer %s open error: %s\n", card,
		   snd_strerror(err));
	    return;
	}
	if ((err = snd_mixer_attach(handle, card)) < 0) {
	    AH_LOG("Mixer attach %s error: %s", card,
		   snd_strerror(err));
	    snd_mixer_close(handle);
	    handle = NULL;
	    return;
	}
	if ((err = snd_mixer_selem_register(handle, NULL, NULL)) < 0) {
	    AH_LOG("Mixer register error: %s", snd_strerror(err));
	    snd_mixer_close(handle);
	    handle = NULL;
	    return;
	}
	err = snd_mixer_load(handle);
	if (err < 0) {
	    AH_LOG("Mixer %s load error: %s", card, snd_strerror(err));
	    snd_mixer_close(handle);
	    handle = NULL;
	    return;
	}
    }
    snd_mixer_selem_id_set_index(sid, 0);
    snd_mixer_selem_id_set_name(sid, conf.volume_control);
    elem = snd_mixer_find_selem(handle, sid);
    if (!elem) {
	AH_LOG("Unable to find simple control '%s',%i\n",
	       snd_mixer_selem_id_get_name(sid),
	       snd_mixer_selem_id_get_index(sid));
	snd_mixer_close(handle);
	handle = NULL;
	return;
    }

    for (chn = 0; chn <= SND_MIXER_SCHN_LAST; chn++) {
	if (!snd_mixer_selem_has_playback_channel(elem, chn))
	    continue;
	if (snd_mixer_selem_has_playback_switch(elem))
	    err = snd_mixer_selem_set_playback_switch(elem, chn, mute);
	else if (mute)
	    rvol = lvol = 0;
	volume = (chn == 1) ? rvol : lvol;
	err = snd_mixer_selem_set_playback_volume(elem, chn, volume);
	if (err < 0) {
	    AH_LOG("Unable to set volume for channel %d\n", chn);
	    snd_mixer_close(handle);
	    handle = NULL;
	    return;
	}
    }
}

void alsa_volume_mono(int chn, int vol, int mute)
{

    static snd_mixer_t *handle = NULL;
    static const char *card = "default";
    snd_mixer_elem_t *elem;
    snd_mixer_selem_id_t *sid;
    int err, ok;

    AH_LOG("%s chn=%d, vol=%d, mute=%d\n",
	   __FUNCTION__, chn, vol, mute);
    snd_mixer_selem_id_alloca(&sid);

    if (handle == NULL) {

	if ((err = snd_mixer_open(&handle, 0)) < 0) {
	    AH_LOG("Mixer %s open error: %s\n", card,
		   snd_strerror(err));
	    return;
	}

	if ((err = snd_mixer_attach(handle, card)) < 0) {
	    AH_LOG("Mixer attach %s error: %s", card,
		   snd_strerror(err));
	    snd_mixer_close(handle);
	    handle = NULL;
	    return;
	}

	if ((err = snd_mixer_selem_register(handle, NULL, NULL)) < 0) {
	    AH_LOG("Mixer register error: %s", snd_strerror(err));
	    snd_mixer_close(handle);
	    handle = NULL;
	    return;
	}

	err = snd_mixer_load(handle);
	if (err < 0) {
	    AH_LOG("Mixer %s load error: %s", card, snd_strerror(err));
	    snd_mixer_close(handle);
	    handle = NULL;
	    return;
	}
    }


    snd_mixer_selem_id_set_index(sid, 0);
    snd_mixer_selem_id_set_name(sid, conf.volume_control);
    elem = snd_mixer_find_selem(handle, sid);
    if (!elem) {
	AH_LOG("Unable to find simple control '%s',%i\n",
	       snd_mixer_selem_id_get_name(sid),
	       snd_mixer_selem_id_get_index(sid));
	snd_mixer_close(handle);
	handle = NULL;
	return;
    }


    ok = snd_mixer_selem_has_playback_channel(elem, chn);

    if (ok) {
	if (snd_mixer_selem_has_playback_switch(elem))
	    err = snd_mixer_selem_set_playback_switch(elem, chn, mute);
	else if (mute)
	    vol = 0;
	err = snd_mixer_selem_set_playback_volume_range(elem, 0, 0x1F);
	if (err < 0)
	    AH_LOG("Unable to set volume scale for channel %d\n", chn);

	err = snd_mixer_selem_set_playback_volume(elem, chn, vol);

	if (err < 0) {
	    AH_LOG("Unable to set volume for channel %d\n", chn);

	    snd_mixer_close(handle);
	    handle = NULL;
	    return;
	}
    }
}

static void alsa_set_threshold (snd_pcm_t *handle, snd_pcm_uframes_t threshold)
{
    int err;
    snd_pcm_sw_params_t *sw_params;

    snd_pcm_sw_params_alloca (&sw_params);

    err = snd_pcm_sw_params_current (handle, sw_params);
    if (err < 0) {
        AH_LOG("Could not fully initialize DAC\n");
        AH_LOG("Failed to get current software parameters\n");
        return;
    }

    err = snd_pcm_sw_params_set_start_threshold (handle, sw_params, threshold);
    if (err < 0) {
        AH_LOG("Could not fully initialize DAC\n");
        AH_LOG("Failed to set software threshold to %ld\n",
	       threshold);
        return;
    }

    err = snd_pcm_sw_params (handle, sw_params);
    if (err < 0) {
        AH_LOG("Could not fully initialize DAC\n");
        AH_LOG("Failed to set software parameters\n");
        return;
    }
}

static void alsa_anal_close (snd_pcm_t **handlep)
{
    int err = snd_pcm_close (*handlep);
    if (err) {
        AH_LOG("Failed to close PCM handle %p\n", *handlep);
    }
    *handlep = NULL;
}

static void alsa_dump_info (struct alsa_params_req *req,
                            struct alsa_params_obt *obt)
{
    AH_LOG("parameter | requested value | obtained value\n");
    AH_LOG("format    |      %10d |     %10d\n", req->fmt, obt->fmt);
    AH_LOG("channels  |      %10d |     %10d\n",
           req->nchannels, obt->nchannels);
    AH_LOG("frequency |      %10d |     %10d\n", req->freq, obt->freq);
    AH_LOG("============================================\n");
    AH_LOG("requested: buffer size %d period size %d\n",
           req->buffer_size, req->period_size);
    AH_LOG("obtained: samples %ld\n", obt->samples);
}

static int alsa_open (int in, struct alsa_params_req *req,
		      struct alsa_params_obt *obt, snd_pcm_t **handlep)
{
    snd_pcm_t *handle;
    snd_pcm_hw_params_t *hw_params;
    int err;
    int size_in_usec;
    unsigned int freq, nchannels;
    const char *pcm_name = in ? conf.pcm_name_in : conf.pcm_name_out;
    snd_pcm_uframes_t obt_buffer_size;
    const char *typ = in ? "ADC" : "DAC";
    snd_pcm_format_t obtfmt;

    AH_LOG("%s\n", __FUNCTION__);

    freq = req->freq;
    nchannels = req->nchannels;
    size_in_usec = req->size_in_usec;

    snd_pcm_hw_params_alloca (&hw_params);

    err = snd_pcm_open (
			&handle,
			pcm_name,
			in ? SND_PCM_STREAM_CAPTURE : SND_PCM_STREAM_PLAYBACK,
			SND_PCM_NONBLOCK
			);
    if (err < 0) {
        AH_LOG("Failed to open `%s':\n", pcm_name);
        return -1;
    }

    /* Close and then open again: volume control seems to only work
     * after the device has been closed once. */
    err = snd_pcm_close(handle);
    if (err < 0) {
	AH_LOG("Failed to close `%s':\n", pcm_name);
	return -1;
    }

    err = snd_pcm_open (
			&handle,
			pcm_name,
			in ? SND_PCM_STREAM_CAPTURE : SND_PCM_STREAM_PLAYBACK,
			SND_PCM_NONBLOCK
			);
    if (err < 0) {
        AH_LOG("Failed to re-open `%s':\n", pcm_name);
        return -1;
    }
    AH_LOG("handle=%p\n", handle);
    err = snd_pcm_hw_params_any (handle, hw_params);
    if (err < 0) {
        AH_LOG("Failed to initialize hardware parameters\n");
        goto err;
    }

    err = snd_pcm_hw_params_set_access (
					handle,
					hw_params,
					SND_PCM_ACCESS_RW_INTERLEAVED
					);
    if (err < 0) {
        AH_LOG("Failed to set access type\n");
        goto err;
    }

    err = snd_pcm_hw_params_set_format (handle, hw_params, req->fmt);
    if (err < 0 && conf.verbose) {
        AH_LOG("Failed to set format %d\n", req->fmt);
    }

    err = snd_pcm_hw_params_set_rate_near (handle, hw_params, &freq, 0);
    if (err < 0) {
        AH_LOG("Failed to set frequency %d\n", req->freq);
        goto err;
    }

    err = snd_pcm_hw_params_set_channels_near (
					       handle,
					       hw_params,
					       &nchannels
					       );
    if (err < 0) {
        AH_LOG("Failed to set number of channels %d\n",
	       req->nchannels);
        goto err;
    }

    if (nchannels != 1 && nchannels != 2) {
	AH_LOG("Can not handle obtained number of channels %d\n",
	       nchannels);
        goto err;
    }

    if (req->buffer_size) {
        unsigned long obt;

        if (size_in_usec) {
            int dir = 0;
            unsigned int btime = req->buffer_size;

            err = snd_pcm_hw_params_set_buffer_time_near (
							  handle,
							  hw_params,
							  &btime,
							  &dir
							  );
            obt = btime;
        }
        else {
            snd_pcm_uframes_t bsize = req->buffer_size;

            err = snd_pcm_hw_params_set_buffer_size_near (
							  handle,
							  hw_params,
							  &bsize
							  );
            obt = bsize;
        }
        if (err < 0) {
            AH_LOG("Failed to set buffer %s to %d\n",
		   size_in_usec ? "time" : "size", req->buffer_size);
            goto err;
        }

        if ((req->override_mask & 2) && (obt - req->buffer_size))
            AH_LOG("Requested buffer %s %u was rejected, using %lu\n",
                   size_in_usec ? "time" : "size", req->buffer_size, obt);
    }

    if (req->period_size) {
        unsigned long obt;

        if (size_in_usec) {
            int dir = 0;
            unsigned int ptime = req->period_size;

            err = snd_pcm_hw_params_set_period_time_near (
							  handle,
							  hw_params,
							  &ptime,
							  &dir
							  );
            obt = ptime;
        }
        else {
            int dir = 0;
            snd_pcm_uframes_t psize = req->period_size;

            err = snd_pcm_hw_params_set_period_size_near (
							  handle,
							  hw_params,
							  &psize,
							  &dir
							  );
            obt = psize;
        }

        if (err < 0) {
            AH_LOG("Failed to set period %s to %d\n",
		   size_in_usec ? "time" : "size", req->period_size);
            goto err;
        }

        if ((req->override_mask & 1) && (obt - req->period_size))
            AH_LOG("Requested period %s %u was rejected, using %lu\n",
                   size_in_usec ? "time" : "size", req->period_size, obt);
    }

    err = snd_pcm_hw_params (handle, hw_params);
    if (err < 0) {
        AH_LOG("Failed to apply audio parameters\n");
        goto err;
    }

    err = snd_pcm_hw_params_get_buffer_size (hw_params, &obt_buffer_size);
    if (err < 0) {
        AH_LOG("Failed to get buffer size\n");
        goto err;
    }

#if 0
    err = snd_pcm_hw_params_get_format (hw_params, &obtfmt);
    if (err < 0) {
        AH_LOG("Failed to get format\n");
        goto err;
    }
#else
    obtfmt = req->fmt;
#endif

    if (alsa_to_audfmt (obtfmt, &obt->fmt, &obt->endianness)) {
        AH_LOG("Invalid format was returned %d\n", obtfmt);
        goto err;
    }

    err = snd_pcm_prepare (handle);
    if (err < 0) {
        AH_LOG("Could not prepare handle %p\n", handle);
        goto err;
    }

    if (!in && conf.threshold) {
        snd_pcm_uframes_t threshold;
        int bytes_per_sec;

        bytes_per_sec = freq << (nchannels == 2);

        switch (obt->fmt) {
        case AUD_FMT_S8:
        case AUD_FMT_U8:
            break;

        case AUD_FMT_S16:
        case AUD_FMT_U16:
            bytes_per_sec <<= 1;
            break;

        case AUD_FMT_S32:
        case AUD_FMT_U32:
            bytes_per_sec <<= 2;
            break;
        }

        threshold = (conf.threshold * bytes_per_sec) / 1000;
        alsa_set_threshold (handle, threshold);
    }

    obt->nchannels = nchannels;
    obt->freq = freq;
    obt->samples = obt_buffer_size;

    *handlep = handle;

    /*     if (conf.verbose && */
    /*         (obt->fmt != req->fmt || */
    /*          obt->nchannels != req->nchannels || */
    /*          obt->freq != req->freq)) { */
    /*         AH_LOG("Audio paramters for %s\n", typ); */
    /*         alsa_dump_info (req, obt); */
    /*     } */

#ifdef AUDIO_DEBUG
    alsa_dump_info (req, obt);
#endif
    return 0;

    err:
    alsa_anal_close (&handle);
    return -1;
}

static void handle_audio_vol_ctrl_remove_cmd(struct audio_helper *ah,
					     uint8_t *buf, size_t len)
{
    snd_ctl_t *ctl;
    int err;

    err = snd_ctl_open(&ctl, "default", 0);
    if (err<0){
        return;
    }

    snd_ctl_elem_id_t *id;
    snd_ctl_elem_id_alloca(&id);
    snd_ctl_elem_id_set_interface(id, SND_CTL_ELEM_IFACE_MIXER);

    snd_ctl_elem_id_set_name(id, volume_control);
    snd_ctl_elem_value_t *control;
    snd_ctl_elem_value_alloca(&control);
    snd_ctl_elem_value_set_id(control, id);

    err = snd_ctl_elem_remove(ctl, id);
    snd_ctl_close(ctl);
}

static void handle_audio_init_cmd(struct audio_helper *ah,
				  uint8_t *buf, size_t len)
{
    uint8_t *ptr = buf;

    AH_LOG("handle_audio_init_cmd\n");

    ptr += 1;

    memcpy(&conf, ptr, sizeof(conf));
    ptr += sizeof(conf);

    conf.pcm_name_in = pcm_name_in;
    conf.pcm_name_out = pcm_name_out;
    conf.volume_control = volume_control;

    sprintf(pcm_name_in, "dsnoop0");
    sprintf(pcm_name_out, "plug:vm-%d", ah->stubdom_id - 1);
    sprintf(volume_control, "vm-%d", ah->stubdom_id - 1);

    /* memcpy(pcm_name_in, ptr, strlen(ptr) + 1); */
    /* AH_LOG("pcm_name_in=%s\n", conf.pcm_name_in); */
    /* ptr += (strlen(ptr) + 1); */

    /* memcpy(pcm_name_out, ptr, strlen(ptr) + 1); */
    /* AH_LOG("pcm_name_out=%s\n", conf.pcm_name_out); */
    /* ptr += (strlen(ptr) + 1); */

    /* memcpy(volume_control, ptr, strlen(ptr) + 1); */
    /* AH_LOG("volume_control=%s\n", conf.volume_control); */
    /* ptr += (strlen(ptr) + 1); */

}

static void handle_audio_alsa_open_cmd(struct audio_helper *ah,
				       uint8_t *buf, size_t len)
{
    uint8_t *ptr = buf;
    int in;
    struct alsa_params_req req;
    struct alsa_params_obt obt;
    snd_pcm_t *handle = 0;
    int ret;

    ptr += 1;

    memcpy(&in, ptr, sizeof(in));
    ptr += sizeof(in);

    memcpy(&req, ptr, sizeof(req));
    ptr += sizeof(req);

    memcpy(&obt, ptr, sizeof(obt));
    ptr += sizeof(obt);

    ret = alsa_open (in, &req, &obt, &handle);

    ptr = buf;

    ptr++;

    memcpy(ptr, &obt, sizeof(obt));
    ptr += sizeof(obt);

    memcpy(ptr, &handle, sizeof(handle));
    ptr += sizeof(handle);

    memcpy(ptr, &ret, sizeof(ret));
    ptr += sizeof(ret);

    v4v_sendto(ah->fd, buf, ptr - buf, 0, &ah->remote_addr);
}

static void handle_audio_volume_cmd(struct audio_helper *ah,
				    uint8_t *buf, size_t len)
{
    uint8_t *ptr = buf;
    int rvol, lvol, mute;

    ptr += 1;

    memcpy(&rvol, ptr, sizeof(int));
    ptr += sizeof(int);

    memcpy(&lvol, ptr, sizeof(int));
    ptr += sizeof(int);

    memcpy(&mute, ptr, sizeof(int));

    alsa_volume(rvol, lvol, mute);
}

static void handle_audio_volume_mono_cmd(struct audio_helper *ah,
					 uint8_t *buf, size_t len)
{
    uint8_t *ptr = buf;
    int chn, vol, mute;

    ptr += 1;

    memcpy(&chn, ptr, sizeof(int));
    ptr += sizeof(int);

    memcpy(&vol, ptr, sizeof(int));
    ptr += sizeof(int);

    memcpy(&mute, ptr, sizeof(int));

    alsa_volume_mono(chn, vol, mute);
}

static void handle_audio_snd_pcm_close_cmd(struct audio_helper *ah,
					   uint8_t *buf, size_t len)
{
    uint8_t *ptr = buf + 1;
    snd_pcm_t *handle;
    int ret;

    memcpy(&handle, ptr, sizeof(handle));

    ret = snd_pcm_close(handle);

    ptr = buf + 1;
    memcpy(ptr, &ret, sizeof(int));
    ptr += sizeof(int);

    v4v_sendto(ah->fd, buf, ptr - buf, 0, &ah->remote_addr);
}

static void handle_audio_snd_pcm_prepare_cmd(struct audio_helper *ah,
					     uint8_t *buf, size_t len)
{
    uint8_t *ptr = buf + 1;
    snd_pcm_t *handle;
    int ret;

    memcpy(&handle, ptr, sizeof(handle));

    ret = snd_pcm_prepare(handle);

    ptr = buf + 1;
    memcpy(ptr, &ret, sizeof(int));
    ptr += sizeof(int);

    v4v_sendto(ah->fd, buf, ptr - buf, 0, &ah->remote_addr);
}

static void handle_audio_snd_pcm_drop_cmd(struct audio_helper *ah,
					  uint8_t *buf, size_t len)
{
    uint8_t *ptr = buf + 1;
    snd_pcm_t *handle;
    int ret;

    memcpy(&handle, ptr, sizeof(handle));

    ret = snd_pcm_drop(handle);

    ptr = buf + 1;
    memcpy(ptr, &ret, sizeof(int));
    ptr += sizeof(int);

    v4v_sendto(ah->fd, buf, ptr - buf, 0, &ah->remote_addr);
}

static void handle_audio_snd_pcm_avail_update_cmd(struct audio_helper *ah,
						  uint8_t *buf, size_t len)
{
    uint8_t *ptr = buf + 1;
    snd_pcm_t *handle;
    int ret;

    memcpy(&handle, ptr, sizeof(handle));

    ret = snd_pcm_avail_update(handle);

    ptr = buf + 1;
    memcpy(ptr, &ret, sizeof(int));
    ptr += sizeof(int);

    v4v_sendto(ah->fd, buf, ptr - buf, 0, &ah->remote_addr);
}

static void handle_audio_snd_pcm_state_cmd(struct audio_helper *ah,
					   uint8_t *buf, size_t len)
{
    uint8_t *ptr = buf + 1;
    snd_pcm_t *handle;
    int ret;

    memcpy(&handle, ptr, sizeof(handle));

    ret = snd_pcm_state(handle);

    ptr = buf + 1;
    memcpy(ptr, &ret, sizeof(int));
    ptr += sizeof(int);

    v4v_sendto(ah->fd, buf, ptr - buf, 0, &ah->remote_addr);
}

static void handle_audio_snd_pcm_resume_cmd(struct audio_helper *ah,
					    uint8_t *buf, size_t len)
{
    uint8_t *ptr = buf + 1;
    snd_pcm_t *handle;
    int ret;

    memcpy(&handle, ptr, sizeof(handle));

    ret = snd_pcm_resume(handle);

    ptr = buf + 1;
    memcpy(ptr, &ret, sizeof(int));
    ptr += sizeof(int);

    v4v_sendto(ah->fd, buf, ptr - buf, 0, &ah->remote_addr);
}

static void handle_audio_snd_pcm_writei_cmd(struct audio_helper *ah,
					    uint8_t *buf, size_t len)
{
    uint8_t *ptr = buf + 1;
    snd_pcm_t *handle;
    int size;
    int ret;

    memcpy(&handle, ptr, sizeof(handle));
    ptr += sizeof(handle);

    memcpy(&size, ptr, sizeof(int));
    ptr += sizeof(int);

    ret = snd_pcm_writei(handle, ptr, size);

    ptr = buf + 1;
    memcpy(ptr, &ret, sizeof(int));
    ptr += sizeof(int);

    v4v_sendto(ah->fd, buf, ptr - buf, 0, &ah->remote_addr);
}

static void handle_audio_snd_pcm_readi_cmd(struct audio_helper *ah,
					   uint8_t *buf, size_t len)
{
    uint8_t *ptr = buf + 1;
    snd_pcm_t *handle;
    int size;
    int ret;

    memcpy(&handle, ptr, sizeof(handle));
    ptr += sizeof(handle);

    memcpy(&size, ptr, sizeof(int));

    ret = snd_pcm_readi(handle, buf + sizeof(int) + 1, size);

    ptr = buf + 1;
    memcpy(ptr, &ret, sizeof(int));
    ptr += sizeof(int);

    if (ret > 0)
      v4v_sendto(ah->fd, buf, ptr - buf + (ret * 4), 0, &ah->remote_addr);
    else
      v4v_sendto(ah->fd, buf, ptr - buf, 0, &ah->remote_addr);
}

/* This helper needs the stubdom_id to be passed as a cmd-line parameter */
int main (int argc, char *argv[])
{
    int ret;
    struct audio_helper *ah = malloc(sizeof(struct audio_helper));
    uint32_t v4v_ring_size = V4V_AUDIO_RING_SIZE;

    if (argc != 2) {
	AH_LOG("wrong syntax: should be ./audio_helper <stubdom_id>\n");
    }

    ah->stubdom_id = atoi(argv[1]);
    AH_LOG("stubdom_id = %d\n", ah->stubdom_id);
    ah->fd = v4v_socket(SOCK_DGRAM);
    if (ah->fd == -1) {
	ret = -1;
	return ret;
    }

    if (ah->stubdom_id > 0) {
	ah->local_addr.port = AUDIO_PORT;
	ah->local_addr.domain = V4V_DOMID_ANY;

	ah->remote_addr.port = V4V_PORT_NONE;
	ah->remote_addr.domain = ah->stubdom_id;

	ret = ioctl(ah->fd, V4VIOCSETRINGSIZE, &v4v_ring_size);
	AH_LOG("%s:%d ioctl=%d\n", __FUNCTION__, __LINE__, ret);

	ret = v4v_bind(ah->fd, &ah->local_addr, ah->stubdom_id);
	if (ret == -1) {
	    return ret;
	}

	while (1) {
	    ret = v4v_recvfrom(ah->fd, ah->io_buf,
			       MAX_V4V_MSG_SIZE, 0, &ah->remote_addr);
	    //AH_LOG("recvfrom = %d, CMD=%d\n", ret, ah->io_buf[0]);

	    switch(ah->io_buf[0]) {
	    case AUDIO_INIT:
		handle_audio_init_cmd(ah, ah->io_buf, ret);
		break;
	    case AUDIO_ALSA_OPEN:
		handle_audio_alsa_open_cmd(ah, ah->io_buf, ret);
		break;
	    case AUDIO_VOLUME:
		handle_audio_volume_cmd(ah, ah->io_buf, ret);
		break;
	    case AUDIO_VOLUME_MONO:
		handle_audio_volume_mono_cmd(ah, ah->io_buf, ret);
		break;
	    case AUDIO_SND_PCM_CLOSE:
		handle_audio_snd_pcm_close_cmd(ah, ah->io_buf, ret);
		break;
	    case AUDIO_SND_PCM_PREPARE:
		handle_audio_snd_pcm_prepare_cmd(ah, ah->io_buf, ret);
		break;
	    case AUDIO_SND_PCM_DROP:
		handle_audio_snd_pcm_drop_cmd(ah, ah->io_buf, ret);
		break;
	    case AUDIO_SND_PCM_AVAIL_UPDATE:
		handle_audio_snd_pcm_avail_update_cmd(ah, ah->io_buf, ret);
		break;
	    case AUDIO_SND_PCM_STATE:
		handle_audio_snd_pcm_state_cmd(ah, ah->io_buf, ret);
		break;
	    case AUDIO_SND_PCM_WRITEI:
		handle_audio_snd_pcm_writei_cmd(ah, ah->io_buf, ret);
		break;
	    case AUDIO_SND_PCM_READI:
		handle_audio_snd_pcm_readi_cmd(ah, ah->io_buf, ret);
		break;
	    case AUDIO_SND_PCM_RESUME:
		handle_audio_snd_pcm_resume_cmd(ah, ah->io_buf, ret);
		break;
	    case AUDIO_VOL_CTRL_REMOVE:
		handle_audio_vol_ctrl_remove_cmd(ah, ah->io_buf, ret);
		break;
	    default:
		AH_LOG("Unknown CMD=%d\n", ah->io_buf[0]);
	    }
	}
    } else {
	AH_LOG("wrong stubdom_id: must be bigger than 0\n");
    }

    free(ah);

    return 0;
}


