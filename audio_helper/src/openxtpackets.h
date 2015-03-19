#ifndef OPENXT_PACKETS_H
#define OPENXT_PACKETS_H

#include "openxtsettings.h"

typedef enum PacketOpCode {

    // Global
    OPENXT_FINI                 = 01, 

    // Init Commands
    OPENXT_INIT_PLAYBACK        = 10,
    OPENXT_INIT_PLAYBACK_ACK    = 11,
    OPENXT_INIT_CAPTURE         = 12,
    OPENXT_INIT_CAPTURE_ACK     = 13,

    // Fini Commands
    OPENXT_FINI_PLAYBACK        = 20,
    OPENXT_FINI_CAPTURE         = 21,

    // Available
    OPENXT_GET_AVAILABLE        = 30,
    OPENXT_GET_AVAILABLE_ACK    = 31,

    // Control 
    OPENXT_ENABLE_VOICE         = 40,  
    OPENXT_DISABLE_VOICE        = 41,  
    OPENXT_SET_VOLUME           = 42,

    // Process
    OPENXT_PLAYBACK             = 50,
    OPENXT_CAPTURE              = 51,
    OPENXT_PLAYBACK_ACK         = 52,
    OPENXT_CAPTURE_ACK          = 53,

} PacketOpCode;

typedef struct OpenBlankPacket {

} OpenBlankPacket;

typedef struct OpenXTInitPlaybackPacket {

    int32_t fmt;
    int32_t freq;
    int32_t nchannels;
    int32_t sample_size;

} OpenXTInitPlaybackPacket;

typedef struct OpenXTInitPlaybackAckPacket {

    int32_t freq;
    int32_t valid;

} OpenXTInitPlaybackAckPacket;

typedef struct OpenXTGetAvailableAckPacket {

    int32_t available;

} OpenXTGetAvailableAckPacket;

typedef struct OpenXTPlaybackPacket {

    int32_t num_samples;
    char samples[MAX_PCM_BUFFER_SIZE];

} OpenXTPlaybackPacket;

typedef struct OpenXTPlaybackAckPacket {

    int32_t written;

} OpenXTPlaybackAckPacket;

typedef struct OpenXTSetVolumePacket {

    int64_t left;
    int64_t right;

} OpenXTSetVolumePacket;

#define PLAYBACK_PACKET_LENGTH(a,b) (sizeof(int32_t) + (a * b))

#endif // OPENXT_PACKETS_H
