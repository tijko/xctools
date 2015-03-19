#include "unittest.h"

#include "openxtv4v.h"
#include "openxtdebug.h"
#include "openxtaudio.h"

////////////////////////////////////////////////////////////////////////////////
// Global Variables                                                           //
////////////////////////////////////////////////////////////////////////////////

int32_t pass = 0;
int32_t fail = 0;

////////////////////////////////////////////////////////////////////////////////
// Tests                                                                      //
////////////////////////////////////////////////////////////////////////////////

typedef struct TestPacket {

    int32_t data1;
    int32_t data2;

} TestPacket;

void test_v4v(void)
{
    V4VConnection *client = NULL;
    V4VConnection *server = NULL;

    // Packets
    V4VPacket snd_packet;
    V4VPacket rcv_packet;
    TestPacket *snd_packet_body = NULL;
    TestPacket *rcv_packet_body = NULL;

    // Start from zero.
    memset(&snd_packet, 0, sizeof(snd_packet));
    memset(&rcv_packet, 0, sizeof(rcv_packet));

    // TODO: The following still needs testing:
    // 
    // - I currently am not testing invalid arguments to openxt_v4v_open. This 
    //   one is hard as V4V doesn't really check things on it's own

    // Make sure the validate function works
    UT_CHECK(openxt_v4v_validate(sizeof(TestPacket)) == true);
    UT_CHECK(openxt_v4v_validate(V4V_MAX_PACKET_BODY_SIZE * 2) == false);

    // Make sure that we hit the correct errors
    UT_CHECK(openxt_v4v_isconnected(NULL) == false);
    UT_CHECK(openxt_v4v_isconnected(NULL) == false);

    // We should not be connected
    UT_CHECK(openxt_v4v_isconnected(client) == false);
    UT_CHECK(openxt_v4v_isconnected(server) == false);

    // Valid setup
    UT_CHECK((client = openxt_v4v_open(V4V_PORT_NONE, V4V_DOMID_ANY, 5001, 0)) != NULL);
    UT_CHECK((server = openxt_v4v_open(5001, V4V_DOMID_ANY, V4V_PORT_NONE, 0)) != NULL);

    // Make sure that we are connected
    UT_CHECK(openxt_v4v_isconnected(client) == true);
    UT_CHECK(openxt_v4v_isconnected(server) == true);

    // Make sure that we hit the correct errors
    UT_CHECK(openxt_v4v_close(NULL) == -EINVAL);
    UT_CHECK(openxt_v4v_close(NULL) == -EINVAL);

    // Make sure that we can close the connection
    UT_CHECK(openxt_v4v_close(client) == 0);
    UT_CHECK(openxt_v4v_close(server) == 0);

    // Setup again
    UT_CHECK((client = openxt_v4v_open(V4V_PORT_NONE, V4V_DOMID_ANY, 5001, 0)) != NULL);
    UT_CHECK((server = openxt_v4v_open(5001, V4V_DOMID_ANY, V4V_PORT_NONE, 0)) != NULL);

    // Make sure that we hit the correct errors
    UT_CHECK(openxt_v4v_set_opcode(NULL, 0) == -EINVAL);
    UT_CHECK(openxt_v4v_set_length(NULL, 0) == -EINVAL);
    UT_CHECK(openxt_v4v_set_length(&snd_packet, V4V_MAX_PACKET_BODY_SIZE * 2) == -EOVERFLOW);

    // Set the length and opcode
    UT_CHECK(openxt_v4v_set_opcode(&snd_packet, 5) == 0);
    UT_CHECK(openxt_v4v_set_opcode(&rcv_packet, 5) == 0);
    UT_CHECK(openxt_v4v_set_length(&snd_packet, sizeof(TestPacket)) == 0);
    UT_CHECK(openxt_v4v_set_length(&rcv_packet, sizeof(TestPacket)) == 0);

    // Make sure that we hit the correct errors
    UT_CHECK(openxt_v4v_get_opcode(NULL) == -EINVAL);
    UT_CHECK(openxt_v4v_get_length(NULL) == -EINVAL);

    // Set the length and opcode
    UT_CHECK(openxt_v4v_get_opcode(&snd_packet) == 5);
    UT_CHECK(openxt_v4v_get_opcode(&rcv_packet) == 5);
    UT_CHECK(openxt_v4v_get_length(&snd_packet) == sizeof(TestPacket));
    UT_CHECK(openxt_v4v_get_length(&rcv_packet) == sizeof(TestPacket));

    // Make sure that we hit the correct errors
    UT_CHECK((snd_packet_body = openxt_v4v_get_body(NULL)) == NULL);    
    UT_CHECK((rcv_packet_body = openxt_v4v_get_body(NULL)) == NULL);    

    // Make sure that we get valid pointers
    UT_CHECK((snd_packet_body = openxt_v4v_get_body(&snd_packet)) != NULL);    
    UT_CHECK((rcv_packet_body = openxt_v4v_get_body(&rcv_packet)) != NULL);    

    // Put data into the packets
    if (snd_packet_body && rcv_packet_body)
    {
        snd_packet_body->data1 = 1;
        snd_packet_body->data2 = 2;

        // Make sure that we hit the correct errors
        UT_CHECK(openxt_v4v_send(NULL, &snd_packet) == -EINVAL);
        UT_CHECK(openxt_v4v_recv(NULL, &rcv_packet) == -EINVAL);
        UT_CHECK(openxt_v4v_send(client, NULL) == -EINVAL);
        UT_CHECK(openxt_v4v_recv(server, NULL) == -EINVAL);

        // Mess up the connection
        UT_CHECK(openxt_v4v_close_internal(client) == 0);
        UT_CHECK(openxt_v4v_close_internal(server) == 0);

        // Make sure that we hit the correct errors
        UT_CHECK(openxt_v4v_send(client, &snd_packet) == -ENODEV);
        UT_CHECK(openxt_v4v_recv(server, &rcv_packet) == -ENODEV);

        // Mess up the length
        snd_packet.header.length = V4V_MAX_PACKET_BODY_SIZE * 2;
        rcv_packet.header.length = V4V_MAX_PACKET_BODY_SIZE * 2;

        // Make sure that we hit the correct errors
        UT_CHECK(openxt_v4v_send(client, &snd_packet) == -EOVERFLOW);

        // Clean things up
        UT_CHECK(openxt_v4v_close(client) == 0);
        UT_CHECK(openxt_v4v_close(server) == 0);
        UT_CHECK((client = openxt_v4v_open(V4V_PORT_NONE, V4V_DOMID_ANY, 5001, 0)) != NULL);
        UT_CHECK((server = openxt_v4v_open(5001, V4V_DOMID_ANY, V4V_PORT_NONE, 0)) != NULL);
        UT_CHECK(openxt_v4v_set_length(&snd_packet, sizeof(TestPacket)) == 0);
        UT_CHECK(openxt_v4v_set_length(&rcv_packet, sizeof(TestPacket)) == 0);

        // Validate that you can send / recv correctly.
        UT_CHECK(openxt_v4v_send(client, &snd_packet) == sizeof(TestPacket));
        UT_CHECK(openxt_v4v_recv(server, &rcv_packet) == sizeof(TestPacket));

        // Validate the result
        UT_CHECK(rcv_packet_body->data1 == 1);
        UT_CHECK(rcv_packet_body->data2 == 2);
    }
}

void test_alsa(void)
{
    Settings settings;
    settings.fmt = SND_PCM_FORMAT_S16_LE;
    settings.freq = 44100;
    settings.nchannels = 2;
    settings.sample_size = 16;

    UT_CHECK(openxt_alsa_mixer_init(&settings) == 0);
    UT_CHECK(openxt_alsa_mixer_init(&settings) == 0);
    UT_CHECK(openxt_alsa_mixer_fini(&settings) == 0);
    UT_CHECK(openxt_alsa_mixer_fini(&settings) == 0);

    UT_CHECK(openxt_alsa_mixer_init(&settings) == 0);
    UT_CHECK(openxt_alsa_set_playback_volume(&settings, -1, -1) == -EINVAL);
    UT_CHECK(openxt_alsa_mixer_init(&settings) == 0);
    UT_CHECK(openxt_alsa_set_playback_volume(&settings, 256, 256) == -EINVAL);

    UT_CHECK(openxt_alsa_mixer_init(&settings) == 0);
    UT_CHECK(openxt_alsa_set_playback_volume(&settings, 0, 0) == 0);
    UT_CHECK(openxt_alsa_set_playback_volume(&settings, 255, 255) == 0);
    UT_CHECK(openxt_alsa_mixer_fini(&settings) == 0);
}


////////////////////////////////////////////////////////////////////////////////
// Support                                                                    //
////////////////////////////////////////////////////////////////////////////////

bool unittest = false;

bool is_unittest(void)
{
    return unittest;
}

void test_run(void)
{
    unittest = true;
    openxt_debug_set_enabled(false);

    // Header
    openxt_info("--------------------------------------------\n");
    openxt_info("- Unit Tests                               -\n");
    openxt_info("--------------------------------------------\n");
    openxt_info("\n");

    // Tests
    test_v4v();
    test_alsa();

    // Footer
    openxt_info("\n");
    openxt_info("\n");
    openxt_info("passed: %d\n", pass);
    openxt_info("failed: %d\n", fail);
    openxt_info("\n");

    openxt_debug_set_enabled(true);
}
