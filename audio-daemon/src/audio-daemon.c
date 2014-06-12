/*
 * audio-daemon.c:
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
#include <pthread.h>

#include "ring.h"
#include "mb.h"
#include "audio-daemon.h"

struct xc_interface *xc_handle = NULL;
struct xen_vsnd_backend *glob_xvb;
char paulian_debug[4];
struct ring_t *cmd_ring = 0;

struct xen_vsnd_device
{
    xen_backend_t backend;
    int domid;
};

static struct event backend_xenstore_event;

/* Backend vsnd operations */
uint64_t get_nsec_now(void)
{
    uint64_t now;
    xc_hvm_get_time(xc_handle, &now);
    return now;
}

void generate_period_interrupt(struct alsa_stream *as)
{
    backend_evtchn_notify(glob_xvb->back, glob_xvb->devid);
}

void *playback_worker_thread(void *arg);
void *capture_worker_thread(void *arg);

static xen_device_t xen_vsnd_alloc(xen_backend_t backend, int devid, void *priv)
{
    struct xen_vsnd_device *dev = priv;
    struct xen_vsnd_backend *xvb;
    int err;

    xvb = (struct xen_vsnd_backend*) malloc(sizeof (*xvb));
    xvb->devid = devid;
    xvb->dev = dev;
    xvb->back = backend;

    glob_xvb = xvb;

    err = pthread_mutex_init(&xvb->p.mutex, NULL);
    err = pthread_mutex_init(&xvb->c.mutex, NULL);

    init_speex();

    return xvb;
}

static int xen_vsnd_init(xen_device_t xendev)
{
    struct xen_vsnd_backend *xvb = xendev;

    backend_print(xvb->back, xvb->devid, "sample-rate", "%d", SAMPLE_RATE);

    return 0;
}

static void xen_vsnd_evtchn_handler(int xvb, short event, void *priv)
{
    backend_evtchn_handler(priv);
}

static int xen_vsnd_connect(xen_device_t xendev)
{
    struct xen_vsnd_backend *xvb = xendev;
    int fd, i;
    uint32_t *page_ref;

    printf("%s\n", __FUNCTION__); fflush(stdout);

    fd = backend_bind_evtchn(xvb->back, xvb->devid);
    if (fd < 0)
        return -1;

    event_set(&xvb->evtchn_event, fd, EV_READ | EV_PERSIST,
              xen_vsnd_evtchn_handler,
              backend_evtchn_priv(xvb->back, xvb->devid));
    event_add(&xvb->evtchn_event, NULL);

    page_ref = xvb->page = backend_map_shared_page(xvb->back, xvb->devid);
    if (!page_ref)
        return -1;


    for (i=0; i<N_AUD_BUFFER_PAGES; i++) {
	xvb->p.dma_buffer[i] = (void *)xc_map_foreign_range(xc_handle, xvb->dev->domid,
							    XENVSND_PAGE_SIZE, PROT_READ | PROT_WRITE,
							    page_ref[100 + i]);
    }
    
    for (i=0; i<N_AUD_BUFFER_PAGES; i++) {
	xvb->c.dma_buffer[i] = (void *)xc_map_foreign_range(xc_handle, xvb->dev->domid,
							    XENVSND_PAGE_SIZE, PROT_READ | PROT_WRITE,
							    page_ref[200 + i]);
    }
    
	/* cmd_ring */
    if (!cmd_ring) {
        printf("MAPPING CMDS RING!\n");
    	cmd_ring = (struct ring_t *) xc_map_foreign_range(xc_handle, xvb->dev->domid,
							  XENVSND_PAGE_SIZE, PROT_READ | PROT_WRITE,
							  page_ref[300]);
	ring_init(cmd_ring);
    }

    xvb->p.be_info = (struct be_info *) &page_ref[400];
	
    xvb->c.be_info = (struct be_info *) &page_ref[500];

    init_alsa(xvb);

    printf("%s exit\n", __FUNCTION__); fflush(stdout);
    return 0;
}


static void xen_vsnd_disconnect(xen_device_t xendev)
{
    struct xen_vsnd_backend *xvb = xendev;
    int i;

    printf("%s\n", __FUNCTION__); fflush(stdout);
    if (xvb->page == NULL)
	return;

    cleanup_alsa(xvb);

    event_del(&xvb->evtchn_event);

    backend_unbind_evtchn(xvb->back, xvb->devid);

    if (xvb->page) {
	backend_unmap_shared_page(xvb->back, xvb->devid, xvb->page);
	xvb->page = NULL;
    }

    for (i=0; i<N_AUD_BUFFER_PAGES; i++) {
	munmap(xvb->p.dma_buffer[i], XENVSND_PAGE_SIZE);
	xvb->p.dma_buffer[i] = NULL;
    }
    
    for (i=0; i<N_AUD_BUFFER_PAGES; i++) {
	munmap(xvb->c.dma_buffer[i], XENVSND_PAGE_SIZE);
	xvb->c.dma_buffer[i] = NULL;
    }

    munmap(cmd_ring, XENVSND_PAGE_SIZE);
    cmd_ring = NULL;

    printf("%s exit\n", __FUNCTION__); fflush(stdout);
}


static void xen_vsnd_event(xen_device_t xendev)
{
    struct xen_vsnd_backend *xvb = xendev;
    struct fe_cmd cmd;
    int len;

    while (1) {
	len = ring_read(cmd_ring, (void *)&cmd, sizeof(cmd));
	if (len == sizeof(cmd)) {

	    printf("(%d) ", cmd.stream);
	    switch(cmd.cmd) {
	    case XC_PCM_OPEN:
	    	printf("OPEN\n");
	    	break;
	    case XC_PCM_CLOSE:
	    	printf("CLOSE\n\n");
	    	break;
	    case XC_PCM_PREPARE:
	    	printf("  PREPARE\n");
	    	break;
	    case XC_TRIGGER_START:
	    	printf("    START\n");
	    	break;
	    case XC_TRIGGER_STOP:
	    	printf("    STOP\n");
	    	break;
	    }

	    if (cmd.stream == XC_STREAM_PLAYBACK)
		process_playback_cmd(&cmd, &xvb->p);
	    else
		process_capture_cmd(&cmd, &xvb->c);	
	} else {
	    return;
	}
    }
}

static void xen_vsnd_free(xen_device_t xendev)
{
    struct xen_vsnd_backend *xvb = xendev;
    struct xen_vsnd_device *dev = xvb->dev;

    xen_vsnd_disconnect(xvb);
    free(xvb);
}


static struct xen_backend_ops xen_vsnd_backend_ops = {
    xen_vsnd_alloc,
    xen_vsnd_init,
    xen_vsnd_connect,
    xen_vsnd_disconnect,
    NULL,
    NULL,
    xen_vsnd_event,
    xen_vsnd_free
};

/* Device creation function */

void xen_vsnd_device_create(int domid)
{
    struct xen_vsnd_device *dev;

    dev = (struct xen_vsnd_device*) malloc(sizeof (*dev));
    dev->domid = domid;
    dev->backend = backend_register("vsnd", domid, &xen_vsnd_backend_ops, dev);
    if (!dev->backend) {
	free(dev);
	return;
    }
}


/* Backend init functions */
static void xen_backend_handler(int fd, short event, void *priv)
{
    backend_xenstore_handler(priv);
}


void xen_backend_init(int dom0)
{
    int rc;

    rc = backend_init(dom0);
    if (rc) {
	printf("Failed to initialize libxenbackend\n");
	return;
    }
    event_set(&backend_xenstore_event,
              backend_xenstore_fd(),
              EV_READ | EV_PERSIST,
              xen_backend_handler,
              NULL);
    event_add(&backend_xenstore_event, NULL);
}

int main(int argc, char *argv[])
{
    int companion = atoi(argv[1]);

    event_init ();

    xc_handle = (struct xc_interface *)xc_interface_open(NULL, NULL, 0);
    if (!xc_handle)
        return -1;

    xen_backend_init (0);
        
    printf("companion domain = %d\n", companion);
    xen_vsnd_device_create(companion); 

    event_dispatch();
	
    return 0;
}

