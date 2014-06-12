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

enum xc_stream {
    XC_STREAM_PLAYBACK = 0,
    XC_STREAM_CAPTURE,    
};

enum xc_cmd {
    XC_PCM_OPEN = 0,
    XC_PCM_CLOSE,
    XC_PCM_PREPARE,
    XC_TRIGGER_START,
    XC_TRIGGER_STOP,
};

enum stream_status {
    STREAM_STOPPED = 0,
    STREAM_STARTING,
    STREAM_STARTED,
    STREAM_STOPPING,
};

struct fe_cmd {
    uint8_t stream;
    uint8_t cmd;
    uint8_t data[6];
    uint64_t s_time;
} __attribute__((packed));

struct be_info {
    int hw_ptr;
    int delay;
    int64_t total_processed;
    uint64_t s_time;
    uint64_t appl_ptr;
    enum stream_status status;
};

#define N_AUD_BUFFER_PAGES 8
#define XENVSND_PAGE_SIZE 4096
#define P_PERIOD_FRAMES 1024
#define P_BUFFER_FRAMES 4096
#define C_PERIOD_FRAMES P_PERIOD_FRAMES
#define C_BUFFER_FRAMES P_BUFFER_FRAMES
#define PERIOD_FRAMES P_PERIOD_FRAMES
#define BUFFER_FRAMES P_BUFFER_FRAMES
#define SAMPLE_RATE            (44100)
#define PERIOD_BYTES            (PERIOD_FRAMES * 4)

struct alsa_stream {
    uint8_t stream_type;
    void *dma_buffer[N_AUD_BUFFER_PAGES];
    struct be_info *be_info;
    int hw_ptr;
    int app_ptr;
    snd_pcm_t *handle;
    snd_pcm_hw_params_t *hwparams;
    snd_pcm_sw_params_t *swparams;
    snd_async_handler_t *ahandler;
    int vol_l;
    int vol_r;
    enum stream_status status;
    pthread_mutex_t mutex;
    int32_t processed;
    int32_t processed_periods;
    uint64_t last_time;
    pthread_t worker_thread;
};

struct xen_vsnd_backend {
    struct xen_vsnd_device *dev;
    xen_backend_t back;
    int devid;

    void *page;
    struct event evtchn_event;

    struct alsa_stream p;
    struct alsa_stream c;
};

struct event audio_work_timer;
void audio_work(int a, short b, void *arg);
