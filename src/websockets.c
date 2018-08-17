#include "../rpc-broker.h"


char *prepare_json_reply(struct json_response *jrsp)
{
    struct json_object *jobj = convert_dbus_response(jrsp);

    if (!jobj)
        return NULL;

    char *reply = malloc(sizeof(char) * WS_RING_BUFFER_MEMBER_SIZE);

    snprintf(reply, WS_RING_BUFFER_MEMBER_SIZE - 1, "%s", 
             json_object_to_json_string(jobj));

    return reply;
}

static int ws_server_callback(struct lws *wsi, enum lws_callback_reasons reason,
                              void *user, void *in, size_t len)
{
    switch (reason) {

        case LWS_CALLBACK_RECEIVE: {
            //
            sem_wait(memory_lock);
            memset(user, '\0', WS_USER_MEM_SIZE); 
            memcpy(user, in, len);

            if (ws_request_handler(wsi, user) == 0)
                lws_callback_on_writable(wsi);
            sem_post(memory_lock);
            //
            break;
        }

        case LWS_CALLBACK_SERVER_WRITEABLE: {

            sem_wait(memory_lock);
            if (lws_ring_get_count_waiting_elements(ring, NULL) > 0) {
                char *rsp = (char *) lws_ring_get_element(ring, NULL);
                memcpy(user + LWS_SEND_BUFFER_PRE_PADDING, rsp, strlen(rsp));

                lws_write(wsi, user + LWS_SEND_BUFFER_PRE_PADDING, 
                          strlen(rsp), LWS_WRITE_TEXT);

                lws_ring_consume(ring, NULL, NULL, 1); 
                lws_callback_on_writable(wsi);
            }
            sem_post(memory_lock);

            break;
        }

        case LWS_CALLBACK_PROTOCOL_INIT:
            DBUS_BROKER_EVENT("<WS client request> %s", "");
            break;

        default:
            break;
    }

    return 0;
}

static struct lws_protocols server_protos[] = {
    
    { "server-callback", ws_server_callback, 0, 0 },
    { NULL, NULL, 0, 0 }
    
};

struct lws_context *create_ws_context(int port)
{
    struct lws_context_creation_info info;
    
    ring = lws_ring_create(WS_RING_BUFFER_MEMBER_SIZE, 
                           WS_RING_BUFFER_MEMBER_NUM, NULL);

    server_protos[0].per_session_data_size = WS_USER_MEM_SIZE;
    
    memset(&info, 0, sizeof(info));
    info.port = port;
    info.protocols = server_protos;

    struct lws_context *context = lws_create_context(&info);

    return context;
}

int ws_request_handler(struct lws *wsi, char *raw_req)
{
    v4v_addr_t addr;
    socklen_t client_len = sizeof(addr);
    int client = lws_get_socket_fd(wsi);

    if (getpeername(client, (struct sockaddr *) &addr, &client_len) < 0) {
        DBUS_BROKER_WARNING("function call failed <%s>", "getpeername");
        return 1;
    }
/*
    struct xs_handle *t = xs_open(XS_OPEN_READONLY);
    if (!t) 
        printf("XENSTORE-FAIL!\n");
    else {
        // query domain
        char *path = xs_get_domain_path(t, addr.domain);
        printf("Domain: %s\n", path);
        free(path);
    }
*/

    // XXX need to hand-off struct lws wsi for call-back
    struct json_request *jreq = convert_json_request(raw_req);

    if (!jreq)
        return 1; 

    jreq->wsi = wsi;

    struct json_response *jrsp = make_json_request(jreq);
    if (!jrsp)
        return 1;

    char *reply = prepare_json_reply(jrsp);
    lws_ring_insert(ring, reply, 1);
    // finish clean-up of structs...

    return 0;
}

