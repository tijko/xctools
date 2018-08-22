#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <stdint.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include "rpc-broker.h"


void *signal_subscription(void *sub)
{
    struct broker_signal *bsig = (struct broker_signal *) sub;
    DBusConnection *conn = bsig->conn;

    while (dbus_connection_get_is_connected(conn)) { 

        sleep(1);
        dbus_connection_read_write(conn, DBUS_REQ_TIMEOUT);
        DBusMessage *msg = dbus_connection_pop_message(conn);

        if (!msg || dbus_message_get_type(msg) != DBUS_MESSAGE_TYPE_SIGNAL)
            continue;

        struct json_response *jrsp = init_jrsp();
        free(jrsp->response_to);
        jrsp->response_to = NULL;
        snprintf(jrsp->type, 7, "signal");
        load_json_response(msg, jrsp);
        jrsp->iface = dbus_message_get_interface(msg);
        jrsp->meth = dbus_message_get_member(msg);
        jrsp->path = dbus_message_get_path(msg);

        char *reply = prepare_json_reply(jrsp);

        lws_ring_insert(ring, reply, 1);
        lws_callback_on_writable(bsig->wsi);
        msg = NULL;
        free_json_response(jrsp);
        dbus_connection_flush(conn);
    }

    dbus_connection_close(conn);
    free(bsig);

    return NULL;
}

void *broker_message(void *request)
{
    struct dbus_request *req = (struct dbus_request *) request;

    int client = req->client;

    fd_set ex_set;
 
    int srv = connect_to_system_bus();

    int bytes = 0;

    do {
        FD_ZERO(&ex_set);
        FD_SET(client, &ex_set);
        FD_SET(srv, &ex_set);

        struct timeval tv = { .tv_sec=1, .tv_usec=0 };
        // set a timeout on the number of times select is able to be called
        int ret = select(srv + 1, &ex_set, NULL, NULL, &tv);

        if (ret == 0)
            continue;

        if (ret < 0)
            DBUS_BROKER_ERROR("select");

        // exchange is passed list
        if (FD_ISSET(srv, &ex_set)) 
            bytes = exchange(srv, client, recv, v4v_send, req);
        else
            bytes = exchange(client, srv, v4v_recv, send, req);

    } while (bytes > 0);

    close(srv);

    // set errno in void *
    return NULL;
}

struct dbus_message *convert_raw_dbus(const char *msg, size_t len)
{
    DBusError error;
    dbus_error_init(&error);
    DBusMessage *dbus_msg = dbus_message_demarshal(msg, len, &error);

    if (dbus_error_is_set(&error)) {
        DBUS_BROKER_WARNING("<De-Marshal failed> [Length: %d] error: %s",
                              len, error.message);
        return NULL;
    }
        
    struct dbus_message *dmsg = malloc(sizeof *dmsg);
    dmsg->dest = dbus_message_get_destination(dbus_msg);

    const char *iface = dbus_message_get_interface(dbus_msg);
    dmsg->iface = iface ? iface : "NULL";

    const char *member = dbus_message_get_member(dbus_msg);
    dmsg->method = member ? member : "NULL";
    // free msg
    return dmsg;
}

int stubdom_check(int domid)
{
    struct xs_handle *xsh = xs_open(XS_OPEN_READONLY);

    size_t len = 1;

    if (!xsh) 
        return -1;

    char *path = xs_get_domain_path(xsh, domid);
    path = realloc(path, sizeof(char) * strlen(path) + 7); 
    strcat(path, "/target");

    int ret = xs_read(xsh, XBT_NULL, path, &len);

    free(path);
    xs_close(xsh);

    return ret;
}

int init_request(int client, struct policy *dbus_policy)
{
    int ret;
    pthread_t dbus_req_thread;
    struct dbus_request *dreq = malloc(sizeof *dreq);
    dreq->client = client;

    v4v_addr_t client_addr;
    size_t client_len = sizeof(client_addr);

    if ((ret = v4v_getpeername(client, &client_addr)) < 0) {
        DBUS_BROKER_WARNING("getpeername call failed <%s>", strerror(errno));
        free(dreq);
        goto request_done;
    }

    dreq->domid = client_addr.domain;
    //
    stubdom_check(dreq->domid);
    //
    dreq->dom_rules = build_domain_policy(dreq->domid, dbus_policy); 

    ret = pthread_create(&dbus_req_thread, NULL, 
                        (void *(*)(void *)) broker_message, (void *) dreq);

request_done:

    return ret;
}

void print_usage(void)
{
    printf("dbus-broker\n");
    printf("\t-b  [--bus-name=BUS]                ");
    printf("A dbus bus name to make the connection to.\n");
    printf("\t-h  [--help]                        ");
    printf("Prints this usage description.\n");
    printf("\t-l  [--logging[=optional FILENAME]  ");
    printf("Enables logging to a default path, optionally set.\n");
    printf("\t-r  [--rule-file=FILENAME]          ");
    printf("Provide a policy file to run against.\n");
    printf("\t-v  [--verbose]                     ");
    printf("Adds extra information (run with logging).\n"); 
}

void sigint_handler(int signal)
{
    /* Catch sighups; allowing rules-file to be reload upon reception */
    /* make the rules structures global?                              */
    DBUS_BROKER_WARNING("<received signal interrupt> %s", "");
    exit(0);
}

void load_json_response(DBusMessage *msg, struct json_response *jrsp)
{
    DBusMessageIter iter, sub;
    dbus_message_iter_init(msg, &iter);

    jrsp->arg_sig = dbus_message_iter_get_signature(&iter);

    struct json_object *args = jrsp->args;

    if (jrsp->arg_sig && jrsp->arg_sig[0] == 'a') {
        dbus_message_iter_recurse(&iter, &sub);
        iter = sub;
        if (jrsp->arg_sig[1] == 'a' || 
            jrsp->arg_sig[1] == 'o' || 
            jrsp->arg_sig[1] == 's' ||
            jrsp->arg_sig[1] == 'i') {
            struct json_object *array = json_object_new_array();
            json_object_array_add(jrsp->args, array);
            args = array;
        }
    }

    parse_signature(args, NULL, &iter);
}

struct json_response *make_json_request(struct json_request *jreq)
{
    struct json_response *jrsp = init_jrsp();

    DBusConnection *conn = jreq->conn;

    /* XXX debug
    printf("ID: %d Destination: %s Path: %s Iface: %s Member: %s ",
            jreq->id, jreq->dmsg->dest, jreq->dmsg->path, jreq->dmsg->iface, jreq->dmsg->method);
    for (int i=0; i < jreq->dmsg->arg_number; i++) {
        switch (jreq->dmsg->arg_sig[i]) {
            case ('i'):
                printf(" Int: %d ", *(int *) jreq->dmsg->args[i]);
                break;
            case ('u'):
                printf(" Uint: %u ", *(uint32_t *) jreq->dmsg->args[i]);
                break;
            case ('s'):
                printf(" String: %s ", (char *) jreq->dmsg->args[i]);
                break;
            case ('b'):
                printf(" Boolean ");
                break;
            case ('v'):
                printf(" Variant ");
                break;
            default:
                printf("Fall-through: %c\n", jreq->dmsg->arg_sig[i]);
                break;
        }
    }
    printf("\n");
    */

    dbus_connection_flush(conn);

    if (jreq->id == 1) {
        const char *busname = dbus_bus_get_unique_name(conn);
        jrsp->id = jreq->id;

        snprintf(jrsp->response_to, JSON_REQ_ID_MAX, JSON_RESP_ID);
        jrsp->arg_sig = "s";
        json_object_array_add(jrsp->args, json_object_new_string(busname));
        return jrsp;
    }
    
    snprintf(jrsp->response_to, JSON_REQ_ID_MAX - 1, "%d", jreq->id);
    DBusMessage *msg = make_dbus_call(conn, jreq->dmsg);

    if (!msg || dbus_message_get_type(msg) == DBUS_MESSAGE_TYPE_ERROR) { 
        DBUS_BROKER_WARNING("response to <%d> request failed", jreq->id); 
        free_json_response(jrsp);
        return NULL;
    }

    load_json_response(msg, jrsp);

    if (strcmp("AddMatch", jreq->dmsg->method) == 0) { 
        pthread_t signal_thr;
        struct broker_signal *bsig = malloc(sizeof *bsig);
        bsig->conn = conn;
        bsig->wsi = jreq->wsi;
        pthread_create(&signal_thr, NULL, signal_subscription, bsig);
    } 

    /* XXX PRINT off arguments from response
    if (jrsp)
        printf("response-to %s %s\n", jrsp->response_to, json_object_to_json_string(jrsp->args));
    */

    return jrsp;
}

void run(struct dbus_broker_args *args)
{
    srand48(time(NULL));
    dbus_broker_policy = build_policy(args->rule_file);

    /* XXX test 
    struct rules *head = dbus_broker_policy->domain_rules;
    while (head) {
        printf("UUID: %s\n", head->uuid);
        struct rule **rule_list = head->rule_list;
        for (int i=0; i < head->count; i++) 
            printf("    %s\n", rule_list[i]->rule_string);
        head = head->next;
    }
    */ 

    // XXX lock-test->
    memory_lock = malloc(sizeof(sem_t));

    sem_init(memory_lock, 0, 1);

    struct dbus_broker_server *server = start_server(BROKER_DEFAULT_PORT);

    DBUS_BROKER_EVENT("<Server has started listening> [Port: %d]",
                        BROKER_DEFAULT_PORT);

    int default_socket = server->dbus_socket;

    fd_set server_set;

    struct lws_context *ws_context = create_ws_context(BROKER_UI_PORT);

    if (!ws_context)
        DBUS_BROKER_ERROR("WebSockets-Server");

    DBUS_BROKER_EVENT("<WebSockets-Server has started listening> [Port: %d]",
                        BROKER_UI_PORT);

    // global state flag?
    while ( 1 ) {

        FD_ZERO(&server_set);
        FD_SET(default_socket, &server_set);

        lws_service(ws_context, WS_LOOP_TIMEOUT);

        struct timeval tv = { .tv_sec=0, .tv_usec=DBUS_BROKER_TIMEOUT };
        int ret = select(default_socket + 1, &server_set, NULL, NULL, &tv);

        if (ret == 0)
            continue;

        if (ret < 0)
            DBUS_BROKER_ERROR("select");

        int client;

        if (FD_ISSET(default_socket, &server_set) == 0)
            continue;
  
        client = v4v_accept(default_socket, &server->peer);
        
        if (client < 0)
            DBUS_BROKER_ERROR("v4v_accept");

        DBUS_BROKER_EVENT("<Client has made a connection> [Dom: %d Client: %d]",
                            server->peer.domain, client);
        init_request(client, dbus_broker_policy);
    }

    free(server);
    free_policy(dbus_broker_policy);
    close(default_socket);

    lws_context_destroy(ws_context);
}

int main(int argc, char *argv[])
{
    const char *dbus_broker_opt_str = "b:hl::r:v";

    struct option dbus_broker_opts[] = {
        { "bus-name",    required_argument, 0, 'b' },
        { "help",        no_argument,       0, 'h' },
        { "logging",     optional_argument, 0, 'l' },
        { "rule-file",   required_argument, 0, 'p' },
        { "verbose",     no_argument,       0, 'v' },
        {  0,            0,        0,           0  }
    };

    int opt;
    int option_index;

    bool logging = false;
    bool verbose = false;

    char *bus_file = NULL;
    char *logging_file = "";
    char *rule_file  = RULES_FILENAME;

    while ((opt = getopt_long(argc, argv, dbus_broker_opt_str,
                              dbus_broker_opts, &option_index)) != -1) {

        switch (opt) {

            case ('b'):
                bus_file = optarg;
                break;

            case ('l'):
                logging = true;
                if (optarg)
                    logging_file = optarg;
                break;

            case ('r'):
                rule_file = optarg;
                break;

            case ('v'):
                verbose = true;
                break;

            case ('h'):
            case ('?'):
                print_usage();
                exit(0);
                break;
        }
    }

    struct dbus_broker_args args = {
        .logging=logging,
        .verbose=verbose,
        .bus_name=bus_file,
        .logging_file=logging_file,
        .rule_file=rule_file,
    };

    struct sigaction sa = { .sa_handler=sigint_handler};

    if (sigaction(SIGINT, &sa, NULL) < 0)
        DBUS_BROKER_ERROR("sigaction");

    run(&args);

    return 0;
}

