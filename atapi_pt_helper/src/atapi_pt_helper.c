/*
 * Copyright (c) 2014 Citrix Systems, Inc.
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

#define ATAPI_HELPER_DEBUG  0
#define ATAPI_HELPER_TAG    "atapi_helper:"
#define DPRINTF(fmt, ...)                                           \
    do {                                                            \
        if (ATAPI_HELPER_DEBUG)                                     \
            fprintf(stderr, ATAPI_HELPER_TAG " %s:%d: " fmt "\n",   \
                    __FILE__, __LINE__, ##__VA_ARGS__);             \
    } while (0)

#define V4V_TYPE 'W'
#define V4VIOCSETRINGSIZE       _IOW (V4V_TYPE,  1, uint32_t)

#define ATAPI_CDROM_PORT 5000

#define V4V_ATAPI_PT_RING_SIZE \
  (V4V_ROUNDUP((((4096)*64) - sizeof(v4v_ring_t)-V4V_ROUNDUP(1))))

#define MAX_V4V_MSG_SIZE (V4V_ATAPI_PT_RING_SIZE)

typedef enum v4vcmd
{
    ATAPI_PT_OPEN      = 0x00,
    ATAPI_PT_CLOSE     = 0x01,
    ATAPI_PT_IOCTL     = 0x02,
    ATAPI_PT_SET_STATE = 0x03,
    ATAPI_PT_GET_STATE = 0x04,

    ATAPI_PT_NUMBER_OF_COMMAND
} ptv4vcmd;

enum block_pt_cmd {
    BLOCK_PT_CMD_ERROR                   = 0x00,
    BLOCK_PT_CMD_SET_MEDIA_STATE_UNKNOWN = 0x01,
    BLOCK_PT_CMD_SET_MEDIA_PRESENT       = 0x02,
    BLOCK_PT_CMD_SET_MEDIA_ABSENT        = 0x03,

    BLOCK_PT_CMD_GET_LASTMEDIASTATE      = 0x04,
    BLOCK_PT_CMD_GET_SHM_MEDIASTATE      = 0x05
};

typedef enum {
    MEDIA_STATE_UNKNOWN = 0x0,
    MEDIA_PRESENT = 0x1,
    MEDIA_ABSENT = 0x2
} ATAPIPTMediaState;

typedef struct ATAPIPTShm {
    ATAPIPTMediaState    mediastate;
} ATAPIPTShm;

#define MAX_ATAPI_PT_DEVICES 6
typedef struct ATAPIPTDeviceState {
    char filename[256];
    int fd;

    uint32_t max_atapi_pt_xfer_len;

    ATAPIPTShm* volatile shm;
    ATAPIPTMediaState lastmediastate;
} ATAPIPTDeviceState;

typedef struct ATAPIPTHelperState {
    int v4vfd;
    v4v_addr_t remote_addr;
    v4v_addr_t local_addr;

    int stubdom_id;

    uint8_t device_number;
    ATAPIPTDeviceState devices[MAX_ATAPI_PT_DEVICES];
} ATAPIPTHelperState;

static int helper_get_device_state(ATAPIPTHelperState* hs,
                                   char const* name, size_t len,
                                   ATAPIPTDeviceState** ds)
{
    int i, j;

    if (!name || !ds) {
        return -1;
    }

    for (i = 0; i < MAX_ATAPI_PT_DEVICES; i++) {
        if (!strncmp(hs->devices[i].filename, name, len)) {
            *ds = &hs->devices[i];
            return i;
        } else if (hs->devices[i].fd == -1) {
            i = j;
        }
    }

    if (j == MAX_ATAPI_PT_DEVICES) {
        return -1;
    }

    *ds = &hs->devices[j];

    memset((*ds)->filename, 0, sizeof ((*ds)->filename));
    memcpy((*ds)->filename, name, len);
    (*ds)->fd = -1;

    return j;
}

static int initialize_helper_state(ATAPIPTHelperState* hs)
{
    uint32_t v4v_ring_size = V4V_ATAPI_PT_RING_SIZE;

    hs->v4vfd = v4v_socket(SOCK_DGRAM);

    if (hs->v4vfd == -1) {
        DPRINTF("unable to create a v4vsocket");
        return -1;
    }

    hs->local_addr.port = ATAPI_CDROM_PORT;
    hs->local_addr.domain = V4V_DOMID_ANY;

    hs->remote_addr.port = V4V_PORT_NONE;
    hs->remote_addr.domain = hs->stubdom_id;

    ioctl(hs->v4vfd, V4VIOCSETRINGSIZE, &v4v_ring_size);
    if (v4v_bind(hs->v4vfd, &hs->local_addr, hs->stubdom_id) == -1) {
        DPRINTF("unable to bind the v4vsocket");
        v4v_close(hs->v4vfd);
        return -1;
    }

    return 0;
}

static int atapi_pt_open(ATAPIPTHelperState* hs, uint8_t* buf, size_t len)
{
    ATAPIPTDeviceState* ds = NULL;
    int device_id = -1;

    device_id = helper_get_device_state(hs, &buf[1], len - 1, &ds);

    if (ds->fd == -1)
        ds->fd = open(ds->filename, O_RDWR | O_NONBLOCK);

    if (device_id == -1 || ds->fd == -1) {
        buf[1] = 'k';
        buf[2] = 'o';
        DPRINTF("device state not found for '%s'", &buf[1]);
    } else {
        buf[1] = 'o';
        buf[2] = 'k';
    }

    buf[3] = device_id;

    DPRINTF("open (%s): send message [%c%c-%d]",
            ds->filename, buf[1], buf[2], buf[3]);
    if (v4v_sendto(hs->v4vfd, buf, 4, 0, &hs->remote_addr) != 4) {
       DPRINTF("unable to send a message to %s", ds->filename);
       return -1;
    }

    return 0;
}

static int atapi_pt_close(ATAPIPTHelperState* hs, uint8_t* buf, size_t len)
{
    ATAPIPTDeviceState* ds = NULL;
    int ret = -1;

    if (buf[1] < MAX_ATAPI_PT_DEVICES) {
        ds = &hs->devices[buf[1]];
        if (ds->fd > -1) {
            close(ds->fd);
            ds->fd = -1;
        }
        ret = 0;
        DPRINTF("device (%s) closed", ds->filename);
    }

    return ret;
}

static void dump_hex(uint8_t* p, size_t len)
{
  int i, j;

  for (i = 0; i < len; i += 16) {
    for (j = 0; (j < 16) && ((i + j) < len); j++) {
      fprintf(stderr, "%02x ", p[i+j]);
    }
    for (; j < 16; j++) {
      fprintf(stderr, "   ");
    }
    fprintf(stderr, " ");
    for (j = 0; (j < 16) && ((i + j) < len); j++) {
      fprintf(stderr, "%c", ((p[i+j] < ' ') || (p[i+j] > 0x7e)) ? '.' : p[i+j]);
    }
    fprintf(stderr, "\n");
  }
}

static int atapi_pt_ioctl(ATAPIPTHelperState* hs, uint8_t* buf, size_t len)
{
    ATAPIPTDeviceState* ds;
    unsigned long int req;
    uint32_t reserved_size;
    int ret;

    struct sg_io_v4* cmd;
    int is_dout;

    if (!(buf[1] < MAX_ATAPI_PT_DEVICES)) {
        return -1;
    }

    ds = &hs->devices[buf[1]];
    if (ds->fd == -1) {
        return -1;
    }

    req  = (buf[2] << 24) & 0xFF000000;
    req |= (buf[3] << 16) & 0x00FF0000;
    req |= (buf[4] <<  8) & 0x0000FF00;
    req |= (buf[5] <<  0) & 0x000000FF;

    switch (req) {
    case SG_GET_RESERVED_SIZE:
        ret = ioctl(ds->fd, req, &reserved_size);
        if (ret == -1) {
            DPRINTF("SG_IO error %s", strerror(errno));
            buf[1] = 'k';
            buf[2] = 'o';
            len = 3;
        } else {
            buf[1] = 'o';
            buf[2] = 'k';
            memcpy(&buf[3], &reserved_size, sizeof(reserved_size));
            DPRINTF("reserved_size = %u", reserved_size);
            len = 7;
        }
        break;
    case SG_IO:
        //dump_hex(buf, len);
        cmd = (struct sg_io_v4*)&buf[6];
        is_dout = (cmd->dout_xfer_len > 0) ? 1 : 0;
        cmd->request = &buf[6] + sizeof(struct sg_io_v4);
        if (is_dout) {
            cmd->dout_xferp = cmd->request + cmd->request_len;
        }
        cmd->response = &buf[6] + sizeof(struct sg_io_v4);
        if (!is_dout) {
            cmd->din_xferp = cmd->response + cmd->max_response_len;
        }

        ret = ioctl(ds->fd, req, cmd);
        if (ret == -1) {
            DPRINTF("SG_IO error %s", strerror(errno));
            buf[1] = 'k';
            buf[2] = 'o';
        } else {
            buf[1] = 'o';
            buf[2] = 'k';
        }

        len = 6 + sizeof (struct sg_io_v4) + cmd->max_response_len;
        if (!is_dout) {
            len += cmd->din_xfer_len;
        }
        break;
    default:
        DPRINTF("IOCTL(0x%08x) not supported", req);
        buf[1] = 'k';
        buf[2] = 'o';
        len = 3;
        break;
    }

    if (v4v_sendto(hs->v4vfd, buf, len, 0, &hs->remote_addr) != len) {
       DPRINTF("unable to send a message to %s", ds->filename);
       return -1;
    }

    return 0;
}

static int atapi_pt_set_state(ATAPIPTHelperState* hs, uint8_t* buf, size_t len)
{
    ATAPIPTDeviceState* ds = NULL;
    uint8_t cmd;

    if (!(buf[1] < MAX_ATAPI_PT_DEVICES)) {
        return -1;
    }

    ds = &hs->devices[buf[1]];
    if (ds->fd == -1) {
        return -1;
    }
    cmd = buf[2];
    buf[1] = 'o';
    buf[2] = 'k';
    switch (cmd) {
    case BLOCK_PT_CMD_SET_MEDIA_STATE_UNKNOWN:
        ds->shm->mediastate = MEDIA_STATE_UNKNOWN;
        break;
    case BLOCK_PT_CMD_SET_MEDIA_PRESENT:
        ds->shm->mediastate = MEDIA_PRESENT;
        ds->lastmediastate = MEDIA_PRESENT;
        break;
    case BLOCK_PT_CMD_SET_MEDIA_ABSENT:
        /* TODO: No media, remove exclusivity lock */
        ds->shm->mediastate = MEDIA_ABSENT;
        ds->lastmediastate = MEDIA_ABSENT;
        break;
    case BLOCK_PT_CMD_ERROR:
    default:
        buf[1] = 'k';
        buf[2] = 'o';
        break;
    }

    if (v4v_sendto(hs->v4vfd, buf, 3, 0, &hs->remote_addr) != 3) {
       DPRINTF("unable to send a message to %s", ds->filename);
       return -1;
    }

    return 0;
}

static int atapi_pt_get_state(ATAPIPTHelperState* hs, uint8_t* buf, size_t len)
{
    ATAPIPTDeviceState* ds = NULL;
    uint8_t cmd;

    if (!(buf[1] < MAX_ATAPI_PT_DEVICES)) {
        return -1;
    }

    ds = &hs->devices[buf[1]];
    if (ds->fd == -1) {
        return -1;
    }
    cmd = buf[2];
    buf[1] = 'o';
    buf[2] = 'k';
    switch (cmd) {
    case BLOCK_PT_CMD_GET_LASTMEDIASTATE:
        buf[3] = ds->lastmediastate;
        break;
    case BLOCK_PT_CMD_GET_SHM_MEDIASTATE:
        buf[3] = ds->shm->mediastate;
        break;
    case BLOCK_PT_CMD_ERROR:
    default:
        buf[1] = 'k';
        buf[2] = 'o';
        break;
    }

    if (v4v_sendto(hs->v4vfd, buf, 3, 0, &hs->remote_addr) != 3) {
       DPRINTF("unable to send a message to %s", ds->filename);
       return -1;
    }

    return 0;
}

static const struct {
    char const* name;
    int (*cmd)(ATAPIPTHelperState* hs, uint8_t* buf, size_t len);
} v4v_atapi_cmds[ATAPI_PT_NUMBER_OF_COMMAND] = {
    [ATAPI_PT_OPEN] = {
        .name = "ATAPI_PT_OPEN",
        .cmd = atapi_pt_open
    },
    [ATAPI_PT_CLOSE] = {
        .name = "ATAPI_PT_CLOSE",
        .cmd = atapi_pt_close
    },
    [ATAPI_PT_IOCTL] = {
        .name = "ATAPI_PT_IOCTL",
        .cmd = atapi_pt_ioctl
    },
    [ATAPI_PT_SET_STATE] = {
        .name = "ATAPI_PT_SET_STATE",
        .cmd = atapi_pt_set_state
    },
    [ATAPI_PT_GET_STATE] = {
        .name = "ATAPI_PT_GET_STATE",
        .cmd = atapi_pt_get_state
    }
};

static void close_helper_state(ATAPIPTHelperState* hs)
{
    uint8_t buf[2];

    if (hs->v4vfd > -1) {
        v4v_close(hs->v4vfd);
    }

    for (buf[1] = 0; buf[1] < MAX_ATAPI_PT_DEVICES; buf[1]++) {
        atapi_pt_close(hs, buf, 2);
    }

    memset(hs, 0, sizeof (ATAPIPTHelperState));
}

static ATAPIPTHelperState* hs_g;

static void signal_handler(int sig)
{
    DPRINTF("handle signal %d", sig);

    close_helper_state(hs_g);
    exit(0);
}

int main(int const argc, char const* const* argv)
{
    int ret = 1;
    ATAPIPTHelperState hs;
    uint8_t io_buf[MAX_V4V_MSG_SIZE];
    uint8_t cmdID;
    int continue_listen = 1;

    DPRINTF("START");
    memset(&hs, 0, sizeof (ATAPIPTHelperState));

    if (argc != 3) {
        DPRINTF("wrong syntax: should be %s <target_id> <stubdom_id>", argv[0]);
        goto main_exit;
    }

    hs.stubdom_id = atoi(argv[2]);

    if (!(hs.stubdom_id > 0)) {
        DPRINTF("wrong stubdom ID (%d) should be greaten than 0", hs.stubdom_id);
        goto main_exit;
    }

    ret = initialize_helper_state(&hs);
    if  (ret) {
        continue_listen = 0;
    }

    signal(SIGINT, signal_handler);
    hs_g = &hs;

    while (continue_listen) {
        DPRINTF("wait for command from stubdom (%d)", hs.stubdom_id);
        memset(io_buf, 0, sizeof (io_buf));
        ret = v4v_recvfrom(hs.v4vfd, io_buf, sizeof (io_buf),
                           0, &hs.remote_addr);

        cmdID = io_buf[0];
        DPRINTF("receive request for command ID (%d)", cmdID);
        if (cmdID < ATAPI_PT_NUMBER_OF_COMMAND) {
            DPRINTF("  command: %s",
                    v4v_atapi_cmds[cmdID].name);
            ret = v4v_atapi_cmds[cmdID].cmd(&hs, io_buf, ret);
            DPRINTF("  command %s return code %d",
                    v4v_atapi_cmds[cmdID].name, ret);
        } else {
            ret = 1;
            continue_listen = 0;
            DPRINTF("Command not supported: 0x%02x", io_buf[0]);
        }
    }

    DPRINTF("Exit");
    close_helper_state(&hs);

main_exit:
    return ret;
}
