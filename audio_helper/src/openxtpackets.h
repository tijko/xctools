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

#ifndef OPENXT_PACKETS_H
#define OPENXT_PACKETS_H

#include "openxtsettings.h"

typedef enum PacketOpCode {

    // Global
    OPENXT_FINI                         = 01,

    // Init Commands
    OPENXT_PLAYBACK_INIT                = 10,
    OPENXT_PLAYBACK_INIT_ACK            = 11,
    OPENXT_CAPTURE_INIT                 = 12,
    OPENXT_CAPTURE_INIT_ACK             = 13,

    // Fini Commands
    OPENXT_PLAYBACK_FINI                = 20,
    OPENXT_CAPTURE_FINI                 = 21,

    // Available
    OPENXT_PLAYBACK_GET_AVAILABLE       = 30,
    OPENXT_PLAYBACK_GET_AVAILABLE_ACK   = 31,
    OPENXT_CAPTURE_GET_AVAILABLE        = 32,
    OPENXT_CAPTURE_GET_AVAILABLE_ACK    = 33,

    // Control
    OPENXT_PLAYBACK_ENABLE_VOICE        = 40,
    OPENXT_PLAYBACK_DISABLE_VOICE       = 41,
    OPENXT_PLAYBACK_SET_VOLUME          = 43,
    OPENXT_CAPTURE_ENABLE_VOICE         = 44,
    OPENXT_CAPTURE_DISABLE_VOICE        = 45,

    // Process
    OPENXT_PLAYBACK                     = 50,
    OPENXT_CAPTURE                      = 51,
    OPENXT_CAPTURE_ACK                  = 53,

} PacketOpCode;

typedef struct  __attribute__((packed)) {

} OpenBlankPacket;

typedef struct  __attribute__((packed)) {

    int32_t fmt;
    int32_t freq;
    int32_t valid;
    int32_t nchannels;

} OpenXTPlaybackInitAckPacket;

typedef struct  __attribute__((packed)) {

    int32_t available;

} OpenXTPlaybackGetAvailableAckPacket;

typedef struct  __attribute__((packed)) {

    int32_t num_samples;
    char samples[MAX_PCM_BUFFER_SIZE];

} OpenXTPlaybackPacket;

typedef struct  __attribute__((packed)) {

    int32_t vol;
    int32_t enabled;

} OpenXTPlaybackSetVolumePacket;

typedef struct  __attribute__((packed)) {

    int32_t fmt;
    int32_t freq;
    int32_t valid;
    int32_t nchannels;

} OpenXTCaptureInitAckPacket;

typedef struct  __attribute__((packed)) {

    int32_t available;

} OpenXTCaptureGetAvailableAckPacket;

typedef struct  __attribute__((packed)) {

    int32_t num_samples;

} OpenXTCapturePacket;

typedef struct  __attribute__((packed)) {

    int32_t num_samples;
    char samples[MAX_PCM_BUFFER_SIZE];

} OpenXTCaptureAckPacket;

#define PLAYBACK_PACKET_LENGTH(a) (sizeof(int32_t) + (sizeof(uint32_t) * a))
#define CAPTURE_ACK_PACKET_LENGTH(a) (sizeof(int32_t) + (sizeof(uint32_t) * a))

#endif // OPENXT_PACKETS_H
