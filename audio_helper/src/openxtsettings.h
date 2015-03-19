#ifndef OPENXT_SETTINGS
#define OPENXT_SETTINGS

#define SYSLOG
#define DEBUGGING_ENABLED
#define TAG "openxt_audio_back"

// The following means that we should have room for roughly 1280 samples 
#define MAX_PCM_BUFFER_SIZE (4096)

// Define the maximum size of a V4V packet
#define V4V_MAX_PACKET_BODY_SIZE (4096 * 2)

// The following is the V4V port that we will use for communications. 
#define OPENXT_AUDIO_PORT 5001

#endif // OPENXT_SETTINGS