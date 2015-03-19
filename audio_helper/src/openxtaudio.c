#include "project.h"

#include "openxtv4v.h"
#include "openxtdebug.h"
#include "openxtaudio.h"
#include "openxtpackets.h"

#include "unittest.h"

////////////////////////////////////////////////////////////////////////////////
// Global Data / Structures                                                   //
////////////////////////////////////////////////////////////////////////////////

Settings playback_settings;
Settings capture_settings;

char pcm_name_in[256];
char pcm_name_out[256];
char volume_control[256];

// Global V4V Packets
V4VPacket snd_packet;
V4VPacket rcv_packet;

// GLobal V4V Connection
V4VConnection *conn = NULL;

// Global V4V Packet Bodies
OpenXTPlaybackPacket *playback_packet = NULL;
OpenXTSetVolumePacket *set_volume_packet = NULL;
OpenXTPlaybackAckPacket *playback_ack_packet = NULL;
OpenXTInitPlaybackPacket *init_playback_packet = NULL;
OpenXTInitPlaybackAckPacket *init_playback_ack_packet = NULL;
OpenXTGetAvailableAckPacket *get_available_ack_packet = NULL;

////////////////////////////////////////////////////////////////////////////////
// Alsa Functions                                                             //
////////////////////////////////////////////////////////////////////////////////

int openxt_alsa_remove_pcm(void)
{
    int ret;
    snd_ctl_t *ctl;
    snd_ctl_elem_id_t *id;

    // Santiy checks
    openxt_assert_quiet(is_unittest() == false, 0);

    // Open the ALSA control interface
    openxt_assert((ret = snd_ctl_open(&ctl, "default", 0)) == 0, ret);
    openxt_assert((ret = snd_ctl_elem_id_malloc(&id)) == 0, ret);
    snd_ctl_elem_id_set_interface(id, SND_CTL_ELEM_IFACE_MIXER);
    snd_ctl_elem_id_set_name(id, volume_control);

    // Remove the PCM
    openxt_assert((ret = snd_ctl_elem_remove(ctl, id)) == 0, ret);

    // Cleanup
    snd_ctl_elem_id_free(id);
    snd_ctl_close(ctl);

    // Success
    return 0;
}

int openxt_alsa_mixer_fini(Settings *settings)
{
    int ret = 0;

    // Sanity checks
    openxt_checkp(settings, -EINVAL);

    // Cleanup
    if (settings->mhandle != NULL) {

        // Remove the PCM
        openxt_alsa_remove_pcm();

        // Close the mixer
        ret = snd_mixer_close(settings->mhandle);
    }

    // Reset
    settings->mhandle = NULL;

    // Done
    openxt_info("alsa mixer closed\n");
    return ret;

}

int openxt_alsa_mixer_init(Settings *settings)
{
    int ret;

    // Sanity checks
    openxt_checkp(settings, -EINVAL);
    openxt_assert_quiet(settings->mhandle == NULL, 0);

    // Setup the handle
    openxt_assert_goto((ret = snd_mixer_open(&settings->mhandle, 0)) == 0, failure);
    openxt_assert_goto((ret = snd_mixer_attach(settings->mhandle, "default")) == 0, failure);
    openxt_assert_goto((ret = snd_mixer_selem_register(settings->mhandle, NULL, NULL)) == 0, failure);
    openxt_assert_goto((ret = snd_mixer_load(settings->mhandle)) == 0, failure);

    // Success
    openxt_info("alsa mixer open\n");
    return 0;

failure:

    // Safety
    openxt_alsa_mixer_fini(settings);

    // Failure
    openxt_error("failed to open alsa mixer: %d - %s\n", ret, snd_strerror(ret));
    return ret;
}

int openxt_alsa_fini(Settings *settings)
{
    int ret = 0;

    // Sanity checks
    openxt_checkp(settings, -EINVAL);

    // Cleanup
    if (settings->handle != NULL) 
        ret = snd_pcm_close(settings->handle);

    // Reset
    settings->handle = NULL;

    // Done
    openxt_info("alsa closed\n");
    return ret;
}

int openxt_alsa_init(Settings *settings, char *name, snd_pcm_stream_t stream)
{
    int ret;
    snd_pcm_hw_params_t *hw_params = NULL;

    // Sanity checks
    openxt_checkp(name, -EINVAL);
    openxt_checkp(settings, -EINVAL);
    openxt_assert_quiet(settings->handle == NULL, 0);

    // ALSA BUG: This is a bug with ALSA, that can be easily reproduced. 
    // Basically when using the "softvol" plugin (like we do), the plugin 
    // ends up locking the PCM when it is created once opened, and this lock 
    // cannot be removed (you get an EPERM when you attempt to unlock). For 
    // now, the easy solution is to open the PCM, close it, and re-open it. 
    // For whatever reason, when you re-open the PCM, it's state is normal. 
    openxt_assert((ret = snd_pcm_open(&(settings->handle), name, stream, 0)) == 0, ret);
    snd_pcm_close(settings->handle);

    // Open the ALSA device for this VM, and allocate an opaque buffer to store
    // the parameters that are used to setup ALSA. Note that a lot of the examples
    // use the alloca version, but we found that it had issues with the stack, so 
    // this code uses the malloc version.
    openxt_assert((ret = snd_pcm_open(&(settings->handle), name, stream, 0)) == 0, ret);
    openxt_assert_goto((ret = snd_pcm_hw_params_malloc(&hw_params)) == 0, failure);

    // Setup the parameters from the settings that were provided to us, as well 
    // as settings that are specific to our environment (things that QEMU 
    // doesn't actually provide us). Note that the freq (rate) setting needs to 
    // be set using the "near" function which could change the value of freq 
    // if it could not match exactly. For this reason, we will ack back this 
    // freq so that QEMU knows what to be set too. 
    openxt_assert_goto((ret = snd_pcm_hw_params_any(settings->handle, hw_params)) == 0, failure);    
    openxt_assert_goto((ret = snd_pcm_hw_params_set_access(settings->handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED)) == 0, failure);    
    openxt_assert_goto((ret = snd_pcm_hw_params_set_format(settings->handle, hw_params, settings->fmt)) == 0, failure);    
    openxt_assert_goto((ret = snd_pcm_hw_params_set_rate_near(settings->handle, hw_params, &settings->freq, 0)) == 0, failure);    
    openxt_assert_goto((ret = snd_pcm_hw_params_set_channels(settings->handle, hw_params, settings->nchannels)) == 0, failure);    
    
    // The following commits these settings.
    openxt_assert_goto((ret = snd_pcm_hw_params(settings->handle, hw_params)) == 0, failure);

    // Cleanup
    snd_pcm_hw_params_free(hw_params);

    // Success
    openxt_info("alsa \"%s\" open\n", snd_pcm_name(settings->handle));
    return 0;

failure:

    // Cleanup
    if (hw_params != NULL)
        snd_pcm_hw_params_free(hw_params);

    // Safety
    openxt_alsa_fini(settings);

    // Failure
    openxt_error("failed to open alsa: %d - %s\n", ret, snd_strerror(ret));
    return -1;
}

int openxt_alsa_prepare(Settings *settings)
{
    // Sanity check
    openxt_checkp(settings, -EINVAL);
    openxt_checkp(settings->handle, -EINVAL);

    // Prepare ALSA
    return snd_pcm_prepare(settings->handle);
}

int openxt_alsa_drop(Settings *settings)
{
    // Sanity check
    openxt_checkp(settings, -EINVAL);
    openxt_checkp(settings->handle, -EINVAL);

    // Prepare ALSA
    return snd_pcm_drop(settings->handle);
}

int openxt_alsa_get_available(Settings *settings)
{
    int ret;

    // Sanity checks
    openxt_checkp(settings, -EINVAL);
    openxt_checkp(settings->handle, -EINVAL);

    // We add this loop here because if we get an EPIPE error, we need to 
    // try again, and this provides an easy way to do that. 
    while(1) {

        if ((ret = snd_pcm_avail(settings->handle)) < 0) {

            // Check for EPIPE (xrun / suspended). If this is the case, we 
            // restart ALSA and try again. 
            if (ret == -EPIPE) {
                openxt_assert((ret = openxt_alsa_prepare(settings)) == 0, ret);
                continue;
            }

            // If we got this far, we got have an error. 
            openxt_error("snd_pcm_avail failed: %d - %s\n", ret, snd_strerror(ret));

            // If this happens, no samples are available
            ret = 0;
        }

        // Done
        break;
    }

    // Return the number of samples
    return ret;
}

int openxt_alsa_writei(Settings *settings, char *buffer, int32_t num, int32_t size)
{
    int ret;

    // Sanity checks
    openxt_checkp(buffer, -EINVAL);
    openxt_checkp(settings, -EINVAL);
    openxt_checkp(settings->handle, -EINVAL);
    openxt_assert(settings->sample_size * num <= size, -EINVAL);

    // No need to run this if we are writing 0 samples
    if (num <= 0)
        return 0;

    // We add this loop here because if we get an EPIPE error, we need to 
    // try again, and this provides an easy way to do that. 
    while(1) {

        if ((ret = snd_pcm_writei(settings->handle, buffer, num)) < 0) {

            // Check for EPIPE (xrun / suspended). If this is the case, we 
            // restart ALSA and try again. 
            if (ret == -EPIPE) {
                openxt_assert((ret = openxt_alsa_prepare(settings)) == 0, ret);
                continue;
            }

            // If we got this far, we got have an error. 
            openxt_error("snd_pcm_writei failed: %d - %s\n", ret, snd_strerror(ret));
            break;
        }

        // Make sure that all of the samples were written
        if (ret != num) {
            openxt_error("snd_pcm_writei dropped samples: %d\n", num - ret);
            break;
        }

        // Done
        break;
    }

    // Done
    return ret;
}

int openxt_alsa_set_playback_volume(Settings *settings, int64_t left, int64_t right)
{
    int ret;
    snd_mixer_elem_t *elem = NULL;
    snd_mixer_selem_id_t *selem_id = NULL;

    // Sanity checks
    openxt_checkp(settings, -EINVAL);
    openxt_checkp(settings->mhandle, -EINVAL);

    // Create the simple element id. Note, originally I figured you could just
    // put this part in the init function, but if you do that, the volume will
    // only change once. In other words, ALSA expects that you will do this 
    // each time you need to change the volume. 
    openxt_assert_goto((ret = snd_mixer_selem_id_malloc(&selem_id)) == 0, failure);
    snd_mixer_selem_id_set_name(selem_id, volume_control);
    snd_mixer_selem_id_set_index(selem_id, 0);

    // Get the element described by the simple element id that was setup in the 
    // init function.
    openxt_checkp_goto((elem = snd_mixer_find_selem(settings->mhandle, selem_id)), failure);

    // Make sure that we have the correct channels. Currently we only support 
    // 2 channels. If we need to support more, we would need to provide better
    // logic here. 
    openxt_assert_goto(snd_mixer_selem_has_playback_channel(elem, SND_MIXER_SCHN_FRONT_LEFT) != 0, failure);
    openxt_assert_goto(snd_mixer_selem_has_playback_channel(elem, SND_MIXER_SCHN_FRONT_RIGHT) != 0, failure);

    // Make sure that the min and max volume for this element is set correctly.
    openxt_assert_goto((ret = snd_mixer_selem_set_playback_volume_range(elem, 0, 255)) == 0, failure);
    openxt_assert_goto(left >= 0 && left <= 255, failure);
    openxt_assert_goto(right >= 0 && right <= 255, failure);

    // Set the volume
    openxt_assert_goto((ret = snd_mixer_selem_set_playback_volume(elem, SND_MIXER_SCHN_FRONT_LEFT, left)) == 0, failure);
    openxt_assert_goto((ret = snd_mixer_selem_set_playback_volume(elem, SND_MIXER_SCHN_FRONT_RIGHT, right)) == 0, failure);  

    // Cleanup
    snd_mixer_selem_id_free(selem_id);

    // Success
    openxt_debug("setting volume: left = %lld, right = %lld\n", left, right);
    return 0;

failure:

    // Cleanup
    if (selem_id != NULL)
        snd_mixer_selem_id_free(selem_id);

    // Safety
    openxt_alsa_mixer_fini(settings);

    // Failure
    openxt_error("failed to set the volume: %d - %s\n", ret, snd_strerror(ret));
    return -1;
}

////////////////////////////////////////////////////////////////////////////////
// Playback Functions                                                         //
////////////////////////////////////////////////////////////////////////////////

static int openxt_process_playback_init(void)
{
    int ret;

    // QEMU should have sent us the settings that it would like us to use 
    // when configuring ALSA. 
    playback_settings.fmt = init_playback_packet->fmt;
    playback_settings.freq = init_playback_packet->freq;
    playback_settings.nchannels = init_playback_packet->nchannels;
    playback_settings.sample_size = init_playback_packet->sample_size;

    // Set the valid bit
    playback_settings.valid = 1;
    playback_settings.valid &= (openxt_alsa_init(&playback_settings, pcm_name_out, SND_PCM_STREAM_PLAYBACK) == 0) ? 1 : 0;
    playback_settings.valid &= (openxt_alsa_mixer_init(&playback_settings) == 0) ? 1 : 0;

    // Setup the ack packet
    openxt_assert((ret = openxt_v4v_set_opcode(&snd_packet, OPENXT_INIT_PLAYBACK_ACK)) == 0, ret);
    openxt_assert((ret = openxt_v4v_set_length(&snd_packet, sizeof(OpenXTInitPlaybackAckPacket))) == 0, ret);

    // Setup the ack body that will be sent back to QEMU. Specifically we need to 
    // tell QEMU what frequency we are actually running at, as well as 
    // if ALSA was actually configured
    init_playback_ack_packet->freq = playback_settings.freq;
    init_playback_ack_packet->valid = playback_settings.valid;

    // Send the ack. 
    openxt_assert((ret = openxt_v4v_send(conn, &snd_packet)) == sizeof(OpenXTInitPlaybackAckPacket), ret);

    // Success
    return 0;
}

static int openxt_process_playback_fini(void)
{
    openxt_alsa_mixer_fini(&playback_settings);
    openxt_alsa_fini(&playback_settings);

    return 0;
}

static int openxt_process_get_available(void)
{
    int ret;

    // Setup the packet. 
    openxt_assert((ret = openxt_v4v_set_opcode(&snd_packet, OPENXT_PLAYBACK_ACK)) == 0, ret);
    openxt_assert((ret = openxt_v4v_set_length(&snd_packet, sizeof(OpenXTGetAvailableAckPacket))) == 0, ret);

    // Fill in the packet's contents. 
    get_available_ack_packet->available = openxt_alsa_get_available(&playback_settings);

    // Send the packet. 
    openxt_assert((ret = openxt_v4v_send(conn, &snd_packet)) == sizeof(OpenXTGetAvailableAckPacket), ret);

    // Success
    return 0;
}

static int openxt_process_playback(void)
{
    openxt_alsa_writei(&playback_settings, playback_packet->samples, playback_packet->num_samples, MAX_PCM_BUFFER_SIZE);

    return 0;
}

static int openxt_process_enable_voice(void)
{
    openxt_alsa_prepare(&playback_settings);

    return 0;
}

static int openxt_process_disable_voice(void)
{
    openxt_alsa_drop(&playback_settings);

    return 0;
}

static int openxt_process_set_volume(void)
{
    openxt_alsa_set_playback_volume(&playback_settings, set_volume_packet->left, set_volume_packet->right);

    return 0;
}

////////////////////////////////////////////////////////////////////////////////
// Main                                                                       //
////////////////////////////////////////////////////////////////////////////////

int main (int argc, char *argv[])
{
    // Local variables
    int32_t opcode = 0;
    int32_t stubdomid = 0;

    openxt_debug_init();

    // Make sure that we have the right number of arguments. 
    if (argc != 2 && argc != 3) {
       openxt_warn("wrong syntax: should be ./audio_helper <stubdom_id>\n");
        exit(-EINVAL);
    }

    // Get the stubdomain's id
    stubdomid = atoi(argv[1]);

    // Set the ALSA device names. These device names exist inside of the 
    // ALSA configuration file, so we need to make sure that they match. To 
    // see where these are being set, look at the audio_helper_start script.
    sprintf(pcm_name_in, "dsnoop0");
    sprintf(pcm_name_out, "plug:vm-%d", stubdomid - 1);
    sprintf(volume_control, "vm-%d", stubdomid - 1);

    // Check to see if unit testing is turned on. If it is, we run the unit 
    // test instead.
    if (argc == 3) {
        test_run();
        exit(0);
    }

    // Default playback settings (in case of an error)
    memset(&playback_settings, 0, sizeof(playback_settings));
    memset(&capture_settings, 0, sizeof(capture_settings));

    // Cleanup memory (safety)
    memset(&snd_packet, 0, sizeof(V4VPacket));
    memset(&rcv_packet, 0, sizeof(V4VPacket));

    // Pointer checks
    openxt_checkp(playback_packet = openxt_v4v_get_body(&rcv_packet), -EINVAL);
    openxt_checkp(set_volume_packet = openxt_v4v_get_body(&rcv_packet), -EINVAL);
    openxt_checkp(playback_ack_packet = openxt_v4v_get_body(&snd_packet), -EINVAL);
    openxt_checkp(init_playback_packet = openxt_v4v_get_body(&rcv_packet), -EINVAL);
    openxt_checkp(init_playback_ack_packet = openxt_v4v_get_body(&snd_packet), -EINVAL);
    openxt_checkp(get_available_ack_packet = openxt_v4v_get_body(&snd_packet), -EINVAL);

    // Size checks
    openxt_assert(openxt_v4v_validate(sizeof(OpenXTPlaybackPacket)) == true, -EINVAL);
    openxt_assert(openxt_v4v_validate(sizeof(OpenXTSetVolumePacket)) == true, -EINVAL);
    openxt_assert(openxt_v4v_validate(sizeof(OpenXTPlaybackAckPacket)) == true, -EINVAL);
    openxt_assert(openxt_v4v_validate(sizeof(OpenXTInitPlaybackPacket)) == true, -EINVAL);
    openxt_assert(openxt_v4v_validate(sizeof(OpenXTInitPlaybackAckPacket)) == true, -EINVAL);
    openxt_assert(openxt_v4v_validate(sizeof(OpenXTGetAvailableAckPacket)) == true, -EINVAL);

    // Setup V4V
    openxt_assert((conn = openxt_v4v_open(OPENXT_AUDIO_PORT, V4V_DOMID_ANY, V4V_PORT_NONE, stubdomid)) != NULL, -EINVAL);

    // Process incoming commands from QEMU in the stubdomain. Once we get a 
    // "fini" command from QEMU, we know that we can stop executing. 
   while (opcode != OPENXT_FINI) {

        int ret;

        // Wait for a packet to come in from V4V
       openxt_assert((ret = openxt_v4v_recv(conn, &rcv_packet)) >= 0, ret);

        // Process the packet
       switch(opcode = openxt_v4v_get_opcode(&rcv_packet)) {

            case OPENXT_FINI:
                break;

            case OPENXT_INIT_PLAYBACK:
                openxt_assert((ret = openxt_process_playback_init()) == 0, ret);
                break;

            case OPENXT_FINI_PLAYBACK:
                openxt_assert((ret = openxt_process_playback_fini()) == 0, ret);
                break;

            case OPENXT_GET_AVAILABLE:
                openxt_assert((ret = openxt_process_get_available()) == 0, ret);
                break;      

            case OPENXT_PLAYBACK:
                openxt_assert((ret = openxt_process_playback()) == 0, ret);
                break;  

            case OPENXT_ENABLE_VOICE:
                openxt_assert((ret = openxt_process_enable_voice()) == 0, ret);
                break;  

            case OPENXT_DISABLE_VOICE:
                openxt_assert((ret = openxt_process_disable_voice()) == 0, ret);
                break;  

            case OPENXT_SET_VOLUME:
                openxt_assert((ret = openxt_process_set_volume()) == 0, ret);
                break;      

            default:
                openxt_warn("unknown packet opcode: %d\n", opcode);
                exit(-EINVAL);
       }
   }

    // Safely shutdown ALSA mixer
    openxt_alsa_mixer_fini(&playback_settings);

    // Safely shutdown ALSA
    openxt_alsa_fini(&playback_settings);
    openxt_alsa_fini(&capture_settings);

    // No more need for debugging
    openxt_info("safely shutdown\n");
    openxt_debug_fini();

    // Done
    return 0;
}


