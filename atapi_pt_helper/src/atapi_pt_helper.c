/*
 * atapi_pt_helper.c:
 *
 * Copyright (c) 2011 Stefano Panella <stefano.panella@citrix.com>,
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
#include <libv4v.h>

#include <scsi/sg.h>
#include <pthread.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <linux/bsg.h>

//#define ATAPI_CDROM_DEBUG

#ifdef ATAPI_CDROM_DEBUG
# define SH_LOG(Args...) printf(Args)
# define SH_HEX_DUMP(buf, len) sh_hex_dump(buf, len)
#else
# define SH_LOG(Args...)
# define SH_HEX_DUMP(buf, len)
#endif

#define IDE_ATAPI_PT_SHM_NAME_TEMPLATE "/xen-atapi-pt-status-%04x:%04x"
#define IDE_ATAPI_PT_EXCLUSIVE_CD_FILE_TEMPLATE "/var/lock/xen-cd-exclusive-%04x:%04x"

#define ATAPI_PT_LOCK_STATE_UNLOCKED		0
#define ATAPI_PT_LOCK_STATE_LOCKED_BY_ME	1
#define ATAPI_PT_LOCK_STATE_LOCKED_BY_OTHER	2

#define V4V_TYPE 'W'
#define V4VIOCSETRINGSIZE       _IOW (V4V_TYPE,  1, uint32_t)

#define ATAPI_CDROM_PORT 5000

#define V4V_ATAPI_PT_RING_SIZE (V4V_ROUNDUP((((4096)*64) - sizeof(v4v_ring_t)-V4V_ROUNDUP(1))))

#define ATAPI_PT_OPEN                        0x00
#define ATAPI_PT_CLOSE                       0x01
#define ATAPI_PT_IOCTL_SG_IO                 0x03
#define ATAPI_PT_IOCTL_SG_GET_RESERVED_SIZE  0x04
#define ATAPI_PT_GET_PHYS_LOCK               0x05
#define ATAPI_PT_GET_PHYS_LOCK_STATE         0x06
#define ATAPI_PT_RELEASE_PHYS_LOCK           0x07
#define ATAPI_PT_SET_GLOB_MEDIA_STATE        0x08
#define ATAPI_PT_GET_GLOB_MEDIA_STATE        0x09

#define MAX_V4V_MSG_SIZE (V4V_ATAPI_PT_RING_SIZE)

typedef enum {
	MEDIA_STATE_UNKNOWN = 0x0,
	MEDIA_PRESENT = 0x1,
	MEDIA_ABSENT = 0x2
} ATAPIPTMediaState;

typedef struct ATAPIPTShm
{
    ATAPIPTMediaState    mediastate;
} ATAPIPTShm;

#define MAX_ATAPI_PT_DEVS 6

struct atapi_pt_helper {
	int atapi_pt_fd;
	v4v_addr_t remote_addr;
	v4v_addr_t local_addr;
	uint8_t io_buf[MAX_V4V_MSG_SIZE];
	int stubdom_id;
};

struct atapi_pt_dev {
	int fd;	
	uint32_t max_atapi_pt_xfer_len;
	int shmfd;
	int lock_fd;
	ATAPIPTShm * volatile shm;
	char lock_file_name[256];
	struct atapi_pt_helper * sh;
};

uint8_t n_registered_devs = 0;
struct atapi_pt_dev *atapi_dev[MAX_ATAPI_PT_DEVS];

static void sh_hex_dump(const void* address, uint32_t len)
{
	const unsigned char* p = address;
	int i, j;
	
	for (i = 0; i < len; i += 16) {
		for (j = 0; j < 16 && i + j < len; j++)
			SH_LOG("%02x ", p[i + j]);
		for (; j < 16; j++)
			SH_LOG("   ");
		SH_LOG(" ");
		for (j = 0; j < 16 && i + j < len; j++)
			SH_LOG("%c", (p[i + j] < ' ' || p[i + j] > 0x7e) ? '.' : p[i + j]);
		SH_LOG("\n");
	}
}

static void handle_atapi_pt_open_cmd(struct atapi_pt_helper *sh,
				     uint8_t *buf, size_t len)
{
	struct stat st;
	char shm_name[256];
	char dev_name[256];
	struct atapi_pt_dev *aptd;

	aptd = malloc(sizeof(struct atapi_pt_dev));
	if (aptd == NULL) {
		SH_LOG("Error allocating the atapi_pt_dev\n");
		exit(1);
	}
	SH_LOG("aptd = %p\n", aptd);

	atapi_dev[n_registered_devs] = aptd;
	aptd->sh = sh;
	aptd->lock_fd=-1;

	memcpy(dev_name, &buf[1], len - 1);

	SH_LOG("opening dev: %s\n", dev_name);
	aptd->fd = open(dev_name, O_RDWR, 0644);
	SH_LOG("aptd->fd=%d\n", aptd->fd);

	if (fstat(aptd->fd, &st)) {
		fprintf(stderr, "Failed to fstat() the atapi-pt device (fd=%d): %s\n", aptd->fd,
			strerror(errno));
		exit(1);
	}
	snprintf(shm_name, sizeof(shm_name)-1, IDE_ATAPI_PT_SHM_NAME_TEMPLATE,
		 major(st.st_rdev), minor(st.st_rdev));
	shm_name[sizeof(shm_name)-1] = '\0';
	SH_LOG("AAA %s\n", shm_name);
	aptd->shmfd = shm_open(shm_name, O_CREAT | O_RDWR, 0666);
	if (aptd->shmfd < 0) {
		SH_LOG("Open ATAPI-PT SHM failed: %s\n", strerror(errno));
		exit(1);
	}
	ftruncate(aptd->shmfd, sizeof(*(aptd->shm)));
	aptd->shm = mmap(NULL, sizeof(*(aptd->shm)), PROT_READ|PROT_WRITE,
			       MAP_SHARED, aptd->shmfd, 0);
	if (aptd->shm == MAP_FAILED) {
		fprintf(stderr, "Map ATAPI-PT SHM failed: %s\n", strerror(errno));
		exit(1);
	}

	snprintf(aptd->lock_file_name, sizeof(shm_name)-1, IDE_ATAPI_PT_EXCLUSIVE_CD_FILE_TEMPLATE,
		 major(st.st_rdev), minor(st.st_rdev));

	if (ioctl(aptd->fd, SG_GET_RESERVED_SIZE, &aptd->max_atapi_pt_xfer_len)) {
		fprintf(stderr, "ATAPI-PT get max xfer len failed: %s\n", strerror(errno));
		exit(1);
	}

	buf[1] = (aptd->max_atapi_pt_xfer_len >> 24) & 0xFF;
	buf[2] = (aptd->max_atapi_pt_xfer_len >> 16) & 0xFF;
	buf[3] = (aptd->max_atapi_pt_xfer_len >> 8) & 0xFF;
	buf[4] = (aptd->max_atapi_pt_xfer_len) >> 0 & 0xFF;

	buf[5] = n_registered_devs;

	v4v_sendto(sh->atapi_pt_fd, buf, 6, 0, &sh->remote_addr);

	n_registered_devs++;
}

static int handle_atapi_pt_ioctl_sg_io_cmd(struct atapi_pt_helper *sh,
				    uint8_t *buf, size_t len)
{
	struct atapi_pt_dev *aptd = atapi_dev[buf[1]];
	struct sg_io_v4 *cmd = (struct sg_io_v4 *)(buf + 2);
	int is_dout, reply_len, ret;

	is_dout = (cmd->dout_xfer_len > 0) ? 1 : 0;

	cmd->request = buf + 2 +  sizeof(struct sg_io_v4);

	if (is_dout) {
		cmd->dout_xferp = cmd->request + cmd->request_len;
	}

	cmd->response = buf + 2 +  sizeof(struct sg_io_v4);

	if (!is_dout) {
		cmd->din_xferp = cmd->response + cmd->max_response_len;
	}

	SH_LOG("CMD=0x%02x len=%d\n", *(uint8_t *)cmd->request, len);
	SH_HEX_DUMP(buf, len);
	SH_LOG("is_OUT = %d\n", is_dout);
	SH_LOG("cmd->request_len = %d\n", cmd->request_len);
	SH_LOG("cmd->max_response_len = %d\n", cmd->max_response_len);

	buf[1] = ioctl(aptd->fd, SG_IO, cmd);

	SH_LOG("ioctl=%d\n", buf[1]);

	reply_len = 2 + sizeof(struct sg_io_v4) + cmd->max_response_len;
	if (!is_dout) {
		reply_len += cmd->din_xfer_len;
		SH_LOG("cmd->din_xfer_len=%d\n", cmd->din_xfer_len);
	}

	SH_LOG("reply_len = %d\n", reply_len);
	SH_LOG("cmd->request_len = %d\n", cmd->request_len);
	SH_LOG("cmd->max_response_len = %d\n", cmd->max_response_len);

	ret = v4v_sendto(sh->atapi_pt_fd, buf, reply_len, 0, &sh->remote_addr);

	SH_HEX_DUMP(buf, reply_len);
	SH_LOG("v4v_sendto=%d\n\n\n", ret);

	return ret;
}

static int get_lock_fd(char *buf)
{
	struct atapi_pt_dev *aptd = atapi_dev[buf[1]];

	if (aptd->lock_fd)
		close(aptd->lock_fd);

	aptd->lock_fd = open( aptd->lock_file_name, O_RDWR | O_CREAT, 0666);

	return aptd->lock_fd;
}

static int handle_atapi_pt_get_phys_lock_cmd(struct atapi_pt_helper *sh,
					     uint8_t *buf, size_t len)
{
	struct flock lock = {0};
	int lock_fd=get_lock_fd(buf);
	int ret;

	if (lock_fd<0) {
		buf[1] = ATAPI_PT_LOCK_STATE_UNLOCKED;
		goto send;
	}

	lock.l_type = F_WRLCK;
	lock.l_whence = SEEK_SET;
	lock.l_start = 0;
	lock.l_len = 0;
	
	if (fcntl(lock_fd, F_SETLK, &lock)) {
		buf[1] = ATAPI_PT_LOCK_STATE_LOCKED_BY_OTHER;
		goto send;
	}

	buf[1] = ATAPI_PT_LOCK_STATE_LOCKED_BY_ME;
send:	
	ret = v4v_sendto(sh->atapi_pt_fd, buf, 2, 0, &sh->remote_addr);
}

static int handle_atapi_pt_get_phys_lock_state_cmd(struct atapi_pt_helper *sh,
						   uint8_t *buf, size_t len)
{
	struct flock lock = {0};
	int lock_fd=get_lock_fd(buf);
	int ret;

	if (lock_fd<0) {
		buf[1] = ATAPI_PT_LOCK_STATE_UNLOCKED;
		goto send;
	}

	lock.l_type = F_WRLCK;
	lock.l_whence = SEEK_SET;
	lock.l_start = 0;
	lock.l_len = 0;
	
	fcntl(lock_fd, F_GETLK, &lock);
	if (lock.l_type == F_UNLCK) {
		buf[1] = ATAPI_PT_LOCK_STATE_UNLOCKED;
		goto send;
	}

	buf[1] = ATAPI_PT_LOCK_STATE_LOCKED_BY_OTHER;
send:	
	ret = v4v_sendto(sh->atapi_pt_fd, buf, 2, 0, &sh->remote_addr);
}

static int handle_atapi_pt_release_phys_lock_cmd(struct atapi_pt_helper *sh,
						 uint8_t *buf, size_t len)
{
	struct flock lock = {0};
	int lock_fd=get_lock_fd(buf);
	
	if (lock_fd<0) return;
	
	lock.l_type = F_UNLCK;
	lock.l_whence = SEEK_SET;
	lock.l_start = 0;
	lock.l_len = 0;
	
	fcntl(lock_fd, F_SETLK, &lock);
}

static int handle_atapi_pt_set_media_state_cmd(struct atapi_pt_helper *sh,
					       uint8_t *buf, size_t len)
{
	struct atapi_pt_dev *aptd = atapi_dev[buf[1]];
	aptd->shm->mediastate = buf[2];
}

static void handle_atapi_pt_get_media_state_cmd(struct atapi_pt_helper *sh,
					       uint8_t *buf, size_t len)
{
	struct atapi_pt_dev *aptd = atapi_dev[buf[1]];

	buf[1] = aptd->shm->mediastate;

	v4v_sendto(sh->atapi_pt_fd, buf, 2, 0, &sh->remote_addr);
}


/* This helper needs the stubdom_id to be passed as a cmd-line parameter */
int main (int argc, char *argv[])
{
	int ret;
	struct atapi_pt_helper *sh = malloc(sizeof(struct atapi_pt_helper));
	uint32_t v4v_ring_size = V4V_ATAPI_PT_RING_SIZE;

	if (argc != 3) {
		SH_LOG("wrong syntax: should be ./atapi_pt_helper <target_domid> <stubdom_id>\n");
	}

	sh->stubdom_id = atoi(argv[2]);
	SH_LOG("stubdom_id = %d\n", sh->stubdom_id);
	sh->atapi_pt_fd = v4v_socket(SOCK_DGRAM);
	if (sh->atapi_pt_fd == -1) {
		ret = -1;
		return ret;
	}
	
	if (sh->stubdom_id > 0) {
		sh->local_addr.port = ATAPI_CDROM_PORT;
		sh->local_addr.domain = V4V_DOMID_ANY;
		
		sh->remote_addr.port = V4V_PORT_NONE;
		sh->remote_addr.domain = sh->stubdom_id;

		ret = ioctl(sh->atapi_pt_fd, V4VIOCSETRINGSIZE, &v4v_ring_size);
		SH_LOG("%s:%d ioctl=%d\n", __FUNCTION__, __LINE__, ret);
		
		ret = v4v_bind(sh->atapi_pt_fd, &sh->local_addr, sh->stubdom_id);
		if (ret == -1) {
			return ret;
		}	
				
		while (1) {
			ret = v4v_recvfrom(sh->atapi_pt_fd, sh->io_buf,
					   MAX_V4V_MSG_SIZE, 0, &sh->remote_addr);
			SH_LOG("recvfrom = %d, CMD=%d\n", ret, sh->io_buf[0]);

			switch(sh->io_buf[0]) {
			case ATAPI_PT_OPEN:
				handle_atapi_pt_open_cmd(sh, sh->io_buf, ret);
				break;
			case ATAPI_PT_IOCTL_SG_IO:
				handle_atapi_pt_ioctl_sg_io_cmd(sh, sh->io_buf, ret);
				break;
			case ATAPI_PT_GET_PHYS_LOCK:
				handle_atapi_pt_get_phys_lock_cmd(sh, sh->io_buf, ret);
				break;
			case ATAPI_PT_GET_PHYS_LOCK_STATE:
				handle_atapi_pt_get_phys_lock_state_cmd(sh, sh->io_buf, ret);
				break;
			case ATAPI_PT_RELEASE_PHYS_LOCK:
				handle_atapi_pt_release_phys_lock_cmd(sh, sh->io_buf, ret);
				break;
			case ATAPI_PT_SET_GLOB_MEDIA_STATE:
				handle_atapi_pt_set_media_state_cmd(sh, sh->io_buf, ret);
				break;
			case ATAPI_PT_GET_GLOB_MEDIA_STATE:
				handle_atapi_pt_get_media_state_cmd(sh, sh->io_buf, ret);
				break;
			default:
				SH_LOG("Unknown CMD=%d\n", sh->io_buf[0]);
			}
		}
	} else {
		SH_LOG("wrong stubdom_id: must be bigger than 0\n");
	}

	free(sh);

	return 0;
}


