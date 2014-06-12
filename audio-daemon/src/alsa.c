/*
 * alsa.c:
 *
 * Copyright (c) 2012 Stefano Panella <stefano.panella@citrix.com>,
 * All rights reserved.
 *
 */

/*
 * Copyright (c) 2012 Citrix Systems, Inc.
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

#include <unistd.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <alsa/asoundlib.h>
#include <getopt.h>
#include <sys/time.h>
#include <math.h>
#include <event.h>
#include <sched.h>
#include <stdint.h>
#include <time.h>
#include <speex/speex_echo.h>
#include <speex/speex_preprocess.h>

#include "audio-daemon.h"
#include "mb.h"

int period_size;
static snd_output_t *output = NULL;
//static char *device = "hw:0,0";
static char *device = "asym0";

static int do_playback_work(struct alsa_stream *as);
static int do_capture_work(struct alsa_stream *as);

int playback_is_running = 0;
int capture_is_running = 0;
SpeexEchoState *echo_state;
SpeexPreprocessState *preprocess_state;

void refresh_be_info(struct alsa_stream *as, int hw_ptr, int delay,
		     uint64_t s_time, int status)
{
    struct be_info *be_info = as->be_info;

    be_info->hw_ptr = hw_ptr;
    be_info->delay = delay;
    be_info->s_time = s_time;
    be_info->status = status;

    wmb();
}

void alsa_refresh_be_playback_info(struct alsa_stream *as, int periods)
{
    uint64_t time_nsec;
    snd_pcm_sframes_t delay;
    int pointer;
    
    time_nsec = get_nsec_now();

    pointer = as->hw_ptr/4;
    pointer -= (1024 * periods);
    if (pointer < 0)
	pointer += (N_AUD_BUFFER_PAGES * 1024);
    pointer %= (N_AUD_BUFFER_PAGES * 1024);

    refresh_be_info(as, pointer, 0, time_nsec, STREAM_STARTED);
}

void alsa_refresh_be_capture_info(struct alsa_stream *as)
{
    uint64_t time_nsec;
    snd_pcm_sframes_t delay;
    
    time_nsec = get_nsec_now();
    refresh_be_info(as, as->hw_ptr/4, 0, time_nsec, STREAM_STARTED);
}

static void get_data_from_sg(int16_t *dst, int size, struct alsa_stream *as)
{
    int16_t *src;
    int val, vol;

    val = 100; //as->vol_l;
    if (val < 1)
	val = 1;

    size = size / 2;
   
    while (size--) {
	src = as->dma_buffer[as->hw_ptr / XENVSND_PAGE_SIZE] + as->hw_ptr % XENVSND_PAGE_SIZE;
	*dst++ = (*src) * val / 100;

	as->processed += 2;
	as->hw_ptr += 2;
	if (as->hw_ptr == XENVSND_PAGE_SIZE * N_AUD_BUFFER_PAGES) {
	    as->hw_ptr = 0;
	}
    }
    return;   
}

static void put_data_to_sg(int16_t *dst, int size, struct alsa_stream *as)
{
    int16_t *src;
    int val, vol;

    val = 100; //as->vol_l;
    if (val < 1)
	val = 1;

    size = size / 2;

    while (size--) {
	src = as->dma_buffer[as->hw_ptr / XENVSND_PAGE_SIZE] + as->hw_ptr % XENVSND_PAGE_SIZE;
	*src = (*dst++) * val / 100;

	as->processed += 2;
	as->hw_ptr += 2;
	if (as->hw_ptr == XENVSND_PAGE_SIZE * N_AUD_BUFFER_PAGES) {
	    as->hw_ptr = 0;
	}
    }
    return;    
}

static int set_hwparams(snd_pcm_t *handle,
			snd_pcm_hw_params_t *params,
			snd_pcm_access_t access,
			int period_frames,
			int buffer_frames)
{
    unsigned int rrate;
    snd_pcm_uframes_t size;
    int err, dir;

    /* choose all parameters */
    err = snd_pcm_hw_params_any(handle, params);
    if (err < 0) {
	printf("Broken configuration for playback: no configurations available: %s\n", snd_strerror(err));
	return err;
    }
    /* set hardware resampling */
    err = snd_pcm_hw_params_set_rate_resample(handle, params, 0);
    if (err < 0) {
	printf("Resampling setup failed for playback: %s\n", snd_strerror(err));
	return err;
    }
    /* set the interleaved read/write format */
    err = snd_pcm_hw_params_set_access(handle, params, access);
    if (err < 0) {
	printf("Access type not available for playback: %s\n", snd_strerror(err));
	return err;
    }
    /* set the sample format */
    err = snd_pcm_hw_params_set_format(handle, params, SND_PCM_FORMAT_S16);
    if (err < 0) {
	printf("Sample format not available for playback: %s\n", snd_strerror(err));
	return err;
    }
    /* set the count of channels */
    err = snd_pcm_hw_params_set_channels(handle, params, 2);
    if (err < 0) {
	printf("Channels count (%i) not available for playbacks: %s\n", 2, snd_strerror(err));
	return err;
    }
    /* set the stream rate */
    rrate = SAMPLE_RATE;
    err = snd_pcm_hw_params_set_rate_near(handle, params, &rrate, 0);
    if (err < 0) {
	printf("Rate %iHz not available for playback: %s\n", SAMPLE_RATE, snd_strerror(err));
	return err;
    }
    if (rrate != SAMPLE_RATE) {
	printf("Rate doesn't match (requested %iHz, get %iHz)\n", SAMPLE_RATE, err);
	return -EINVAL;
    }
    err = snd_pcm_hw_params_set_buffer_size(handle, params, buffer_frames);
    if (err < 0) {
	printf("Unable to set buffer time %i for playback: %s\n", 2048, snd_strerror(err));
	return err;
    }
    err = snd_pcm_hw_params_get_buffer_size(params, &size);
    if (err < 0) {
	printf("Unable to get buffer size for playback: %s\n", snd_strerror(err));
	return err;
    }
    err = snd_pcm_hw_params_set_period_size(handle, params, period_frames, 0);
    if (err < 0) {
	printf("Unable to set period time %i for playback: %s\n", period_frames, snd_strerror(err));
	return err;
    }
    err = snd_pcm_hw_params_get_period_size(params, &size, &dir);
    if (err < 0) {
	printf("Unable to get period size for playback: %s\n", snd_strerror(err));
	return err;
    }
    /* write the parameters to device */
    err = snd_pcm_hw_params(handle, params);
    if (err < 0) {
	printf("Unable to set hw params for playback: %s\n", snd_strerror(err));
	return err;
    }
    return 0;
}

static int set_swparams(snd_pcm_t *handle, snd_pcm_sw_params_t *swparams)
{
    int err;

    /* get the current swparams */
    err = snd_pcm_sw_params_current(handle, swparams);
    if (err < 0) {
	printf("Unable to determine current swparams for playback: %s\n", snd_strerror(err));
	return err;
    }
    /* start the transfer as soon there is something in the buffer: */
    err = snd_pcm_sw_params_set_start_threshold(handle, swparams, 1);
    if (err < 0) {
	printf("Unable to set start threshold mode for playback: %s\n", snd_strerror(err));
	return err;
    }
    err = snd_pcm_sw_params_set_period_event(handle, swparams, 1);
    if (err < 0) {
	printf("Unable to set period event: %s\n", snd_strerror(err));
	return err;
    }
    /* set timestamp mode */
    err = snd_pcm_sw_params_set_tstamp_mode(handle, swparams, 1);
    if (err < 0) {
	printf("Unable to set avail min for playback: %s\n", snd_strerror(err));
	return err;
    }
    /* write the parameters to the playback device */
    err = snd_pcm_sw_params(handle, swparams);
    if (err < 0) {
	printf("Unable to set sw params for playback: %s\n", snd_strerror(err));
	return err;
    }
    return 0;
}

static int xrun_recovery(snd_pcm_t *handle, int err)
{
    printf("stream recovery\n");
    if (err == -EPIPE) {	/* under-run */
	err = snd_pcm_prepare(handle);
	if (err < 0)
	    printf("Can't recovery from underrun, prepare failed: %s\n", snd_strerror(err));
	return 0;
    } else if (err == -ESTRPIPE) {
	while ((err = snd_pcm_resume(handle)) == -EAGAIN)
	    sleep(1);	/* wait until the suspend flag is released */
	if (err < 0) {
	    err = snd_pcm_prepare(handle);
	    if (err < 0)
		printf("Can't recovery from suspend, prepare failed: %s\n", snd_strerror(err));
	}
	return 0;
    }
    return err;
}

static int alsa_prepare(struct alsa_stream *as)
{
    pthread_mutex_lock(&as->mutex);   
    snd_pcm_prepare(as->handle);
    as->hw_ptr = as->processed = as->processed_periods = 0;
    pthread_mutex_unlock(&as->mutex);   
    return 0;
}


int alsa_get_live_frames(struct alsa_stream *as)
{
    int app_ptr, pv_avail;
    
    rmb();
    app_ptr = as->be_info->appl_ptr;
    while (app_ptr != as->be_info->appl_ptr)
	app_ptr=as->be_info->appl_ptr;

    pv_avail = app_ptr - as->processed/4;
    return pv_avail;
}

char null_buffer[4096] = {0};
char prev_buf_1[4096] = {0};
char prev_buf_2[4096] = {0};
char mono_input[4096] = {0};

static void fill_averege(int16_t *src, int16_t *dst)
{
    int i;
    for (i=0; i<2048; i+=2) {
	dst[i/2] = (src[i] + src[i+1])/2;
    }
}

static void double_mono(int16_t *src, int16_t *dst)
{
    int i;
    for (i=0; i<2048; i+=2) {
	dst[i] = dst[i+1] = src[i/2];
    }
}

int number = 0;
static void alsa_repare(struct xen_vsnd_backend *xvb)
{
    snd_pcm_drop(xvb->p.handle);
    snd_pcm_drop(xvb->c.handle);
    snd_pcm_resume(xvb->p.handle);
    snd_pcm_resume(xvb->c.handle);
    snd_pcm_prepare(xvb->p.handle);
    snd_pcm_prepare(xvb->c.handle);
    number = 0;
    snd_pcm_start(xvb->c.handle);
}

static void capture_callback(snd_async_handler_t *ahandler)
{
    struct xen_vsnd_backend *xvb = snd_async_handler_get_callback_private(ahandler);
    char orig_input[4096];
    char clean_input[4096];
    char output_frame[4096];
    int generate_period = 0;
    struct alsa_stream *as;
    int read, written;
    int avail;

    if (number == 0) {
    	written = snd_pcm_writei(xvb->p.handle, null_buffer, 1024);
    	written = snd_pcm_writei(xvb->p.handle, null_buffer, 1024);
    	written = snd_pcm_writei(xvb->p.handle, null_buffer, 1024);
    	number = 1;
    }

    as = &xvb->c;
    avail = snd_pcm_avail(as->handle);
    if (avail < 0) {
	printf("restarting for avail=%d\n", avail);
	alsa_repare(xvb);
	return;
    }

    if (avail < 1024)
	return;

    read = snd_pcm_readi(as->handle, orig_input, PERIOD_FRAMES);
    if (read < 0) {
	printf("restarting for read=%d\n", read);
	alsa_repare(xvb);
	return;
    }
    pthread_mutex_lock(&as->mutex);
    if (capture_is_running > 1) {

	fill_averege(orig_input, mono_input);

	speex_echo_playback(echo_state, prev_buf_2); 
	speex_echo_capture(echo_state, mono_input, clean_input); 
	speex_preprocess_run(preprocess_state, clean_input); 

	double_mono(clean_input, orig_input);

	put_data_to_sg((uint16_t *)orig_input, read * 4, as);
	alsa_refresh_be_capture_info(as);
	generate_period = 1;
    } else if (capture_is_running == 1){
	capture_is_running = 2;
	/* nothing else to do */
    }
    pthread_mutex_unlock(&as->mutex);

    as = &xvb->p;
    avail = snd_pcm_avail(as->handle);
    if (avail < 0) {
	printf("restarting for avail=%d\n", avail);
	alsa_repare(xvb);
	return;
    }
    pthread_mutex_lock(&as->mutex);
    if ((playback_is_running == 0) || (alsa_get_live_frames(as) < 1024) ) {
	memcpy(output_frame, null_buffer, 4096);
    } else {
	get_data_from_sg((uint16_t *)output_frame, PERIOD_FRAMES * 4, as);
 
	if(playback_is_running < 2) {
	    playback_is_running++;
	} else {
	    alsa_refresh_be_playback_info(as, 1);
	    generate_period = 1;
	}
    }
    pthread_mutex_unlock(&as->mutex);

    written = snd_pcm_writei(as->handle, output_frame, PERIOD_FRAMES);
    if (written < 0) {
	printf("restarting for snd_pcm_writei: written=%d\n", written);
	alsa_repare(xvb);
	return;
    }
    memcpy(prev_buf_2, prev_buf_1, 2048);
    fill_averege(output_frame, prev_buf_1);

    if (generate_period)
	generate_period_interrupt();

}


int alsa_open(struct alsa_stream *as, struct xen_vsnd_backend *xvb)
{
    int err;
    int period_frames;
    int buffer_frames;

    pthread_mutex_lock(&as->mutex);

    snd_pcm_hw_params_alloca(&as->hwparams);
    snd_pcm_sw_params_alloca(&as->swparams);

    err = snd_output_stdio_attach(&output, stdout, 0);
    if (err < 0) {
	printf("Output failed: %s\n", snd_strerror(err));
	return 0;
    }

    if (as->stream_type == XC_STREAM_PLAYBACK) {
	period_frames = P_PERIOD_FRAMES;
	buffer_frames = P_BUFFER_FRAMES;
	if ((err = snd_pcm_open(&as->handle, device, SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
	    printf("Playback open error: %s\n", snd_strerror(err));
	    return 0;
	}
    } else {
	period_frames = C_PERIOD_FRAMES;
	buffer_frames = C_BUFFER_FRAMES;
	if ((err = snd_pcm_open(&as->handle, device, SND_PCM_STREAM_CAPTURE, 0)) < 0) {
	    printf("Capture open error: %s\n", snd_strerror(err));
	    return 0;
	}
    }

    if ((err = set_hwparams(as->handle, as->hwparams, SND_PCM_ACCESS_RW_INTERLEAVED,
			    period_frames, buffer_frames)) < 0) {
	printf("Setting of p_hwparams failed: %s\n", snd_strerror(err));
	exit(EXIT_FAILURE);
    }
    if ((err = set_swparams(as->handle, as->swparams)) < 0) {
	printf("Setting of p_swparams failed: %s\n", snd_strerror(err));
	exit(EXIT_FAILURE);
    }

    if (as->stream_type == XC_STREAM_PLAYBACK) {
    	/* nothing to do, we only rely on the capture callback */
    } else {
    	err = snd_async_add_pcm_handler(&as->ahandler, as->handle, capture_callback, xvb);
    	if (err < 0) {
    	    printf("Unable to register async handler\n");
    	    exit(EXIT_FAILURE);
    	}
    }

    //snd_pcm_dump(as->handle, output);
    pthread_mutex_unlock(&as->mutex);
}

void init_alsa(struct xen_vsnd_backend *xvb)
{
    struct alsa_stream *as;
    int written;

    printf("init_alsa\n");

    as = &xvb->p;
    as->stream_type = XC_STREAM_PLAYBACK;
    alsa_open(as, xvb);
    alsa_prepare(as);

    as = &xvb->c;
    as->stream_type = XC_STREAM_CAPTURE;
    alsa_open(as, xvb);
    alsa_prepare(as);

    //snd_pcm_link(xvb->c.handle, xvb->p.handle);

    snd_pcm_start(xvb->c.handle);
}

void cleanup_alsa(struct xen_vsnd_backend *xvb)
{
    struct alsa_stream *as;

    printf("cleanup_alsa\n");

    as = &xvb->p;
    as->stream_type = XC_STREAM_PLAYBACK;
    snd_pcm_close(as->handle);

    as = &xvb->c;
    as->stream_type = XC_STREAM_CAPTURE;
    snd_pcm_close(as->handle);
}

void init_speex()
{
    int rate=44100;
    spx_int32_t tmp;

    echo_state = speex_echo_state_init(1024, 8192);
    speex_echo_ctl(echo_state, SPEEX_ECHO_SET_SAMPLING_RATE, &rate);

    preprocess_state = speex_preprocess_state_init(1024, 44100);

    tmp = 1;
    speex_preprocess_ctl(preprocess_state, SPEEX_PREPROCESS_SET_AGC, &tmp);

    tmp = 1;
    speex_preprocess_ctl(preprocess_state, SPEEX_PREPROCESS_SET_DENOISE, &tmp);

    tmp = -60;
    speex_preprocess_ctl(preprocess_state, SPEEX_PREPROCESS_SET_ECHO_SUPPRESS, &tmp);
    
    tmp = -60;
    speex_preprocess_ctl(preprocess_state, SPEEX_PREPROCESS_SET_ECHO_SUPPRESS_ACTIVE, &tmp);

    speex_preprocess_ctl(preprocess_state, SPEEX_PREPROCESS_SET_ECHO_STATE, echo_state);  
  
}

void process_playback_cmd(struct fe_cmd *fe_cmd, struct alsa_stream *as)
{
    int ret;

    pthread_mutex_lock(&as->mutex);   
    switch (fe_cmd->cmd) {
    case XC_PCM_OPEN:
	playback_is_running = 0;
	break;
    case XC_PCM_CLOSE:
	playback_is_running = 0;
	break;
    case XC_PCM_PREPARE:
	playback_is_running = 0;
	as->hw_ptr = as->processed = as->processed_periods = 0;
	break;
    case XC_TRIGGER_START:
	refresh_be_info(as, 0, 0, 0, STREAM_STARTING);
	generate_period_interrupt();
	playback_is_running = 1;
	break;
    case XC_TRIGGER_STOP:
	refresh_be_info(as, 0, 0, 0, STREAM_STOPPED);
	generate_period_interrupt();
	playback_is_running = 0;
	break;
    }
    pthread_mutex_unlock(&as->mutex);   
}

void process_capture_cmd(struct fe_cmd *fe_cmd, struct alsa_stream *as)
{
    int ret;

    pthread_mutex_lock(&as->mutex);   
    switch (fe_cmd->cmd) {
    case XC_PCM_OPEN:
	capture_is_running = 0;
	break;
    case XC_PCM_CLOSE:
	capture_is_running = 0;
	break;
    case XC_PCM_PREPARE:
	capture_is_running = 0;
	as->hw_ptr = as->processed = as->processed_periods = 0;
	break;
    case XC_TRIGGER_START:
	refresh_be_info(as, 0, 0, 0, STREAM_STARTING);
	generate_period_interrupt();
	capture_is_running = 1;
	break;
    case XC_TRIGGER_STOP:
	refresh_be_info(as, 0, 0, 0, STREAM_STOPPED);
	generate_period_interrupt();
	capture_is_running = 0;
	break;
    }
    pthread_mutex_unlock(&as->mutex);   
}

