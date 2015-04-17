//
// Copyright (c) 2015 Assured Information Security, Inc
//
// Dates Modified:
//  - 4/8/2015: Initial commit
//    Rian Quinn <quinnr@ainfosec.com>
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//

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

V4VConnection *openxt_v4v_open(int32_t lport, int32_t ldomid, int32_t rport, int32_t rdomid);
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
