#ifndef OPENXT_V4V_H
#define OPENXT_V4V_H

#include <libv4v.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/ioctl.h>

#include "openxtsettings.h"

typedef struct V4VConnection {

    int fd;
    bool connected;
    v4v_addr_t local_addr;
    v4v_addr_t remote_addr;

} V4VConnection;

V4VConnection *openxt_v4v_open(int32_t localport, int32_t localdomid, int32_t remoteport, int32_t remotedomid);
int openxt_v4v_close_internal(V4VConnection *conn);
int openxt_v4v_close(V4VConnection *conn);
bool openxt_v4v_isconnected(V4VConnection *conn);

typedef struct V4VPacketHeader {

    int32_t opcode;
    int32_t length;

} V4VPacketHeader;

typedef struct V4VPacketBody {

    char buffer[V4V_MAX_PACKET_BODY_SIZE];

} V4VPacketBody;

typedef struct V4VPacket {

    V4VPacketHeader header;
    V4VPacketBody body;

} V4VPacket;

bool openxt_v4v_validate(int32_t size);

int openxt_v4v_set_opcode(V4VPacket *packet, int32_t opcode);
int openxt_v4v_set_length(V4VPacket *packet, int32_t length);
int32_t openxt_v4v_get_opcode(V4VPacket *packet);
int32_t openxt_v4v_get_length(V4VPacket *packet);
void *openxt_v4v_get_body(V4VPacket *packet);

int openxt_v4v_send(V4VConnection *conn, V4VPacket *packet);
int openxt_v4v_recv(V4VConnection *conn, V4VPacket *packet);

#endif // OPENXT_V4V_H
