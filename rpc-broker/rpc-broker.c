/*
 Copyright (c) 2018 AIS, Inc.

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <pthread.h>
#include <signal.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include "rpc-broker.h"


/*
 * Whenever clients connect to port 5555, this function will then connect 
 * directly to the DBus system bus socket (/var/run/dbus/system_bus_socket).
 *
 * Polling on the client & server file-descriptors until the connection 
 * communication is finished.
 */
void *broker_message(void *request)
{
    if (!request)
        return NULL;

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
        // Poll on read-ready
        int ret = select(srv + 1, &ex_set, NULL, NULL, &tv);

        if (ret == 0)
            continue;
        if (ret < 0)
            DBUS_BROKER_ERROR("select");

        // Depending on which fd is ready to be read, determines which
        // function pointer to pass to `exchange` 
        if (FD_ISSET(srv, &ex_set))
            bytes = exchange(srv, client, recv, v4v_send, req);
        else
            bytes = exchange(client, srv, v4v_recv, send, req);

    } while (bytes > 0);

    close(srv);
    free(req);

    return NULL;
}

signed int is_stubdom(uint16_t domid)
{
    struct xs_handle *xsh = xs_open(XS_OPEN_READONLY);

    size_t len = 0;

    if (!xsh)
        return -1;

    char *path = xs_get_domain_path(xsh, domid);

    path = realloc(path, strlen(path) + XENSTORE_TARGET_LEN);
    strcat(path, XENSTORE_TARGET);

    void *ret = xs_read(xsh, XBT_NULL, path, &len);

    if (ret)
       free(ret);

    free(path);
    xs_close(xsh);

    return len;
}

int init_request(int client)
{
    int ret;
    pthread_t dbus_req_thread;

    struct dbus_request *dreq = malloc(sizeof *dreq);
    dreq->client = client;

    v4v_addr_t client_addr = { .domain=0, .port=0 };
    
    if ((ret = v4v_getpeername(client, &client_addr)) < 0) {
        DBUS_BROKER_WARNING("getpeername call failed <%s>", strerror(errno));
        free(dreq);
        return ret;
    }

    dreq->domid = client_addr.domain;
    ret = pthread_create(&dbus_req_thread, NULL,
                        (void *(*)(void *)) broker_message, (void *) dreq);

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

static void reload_policy(void *arg)
{
    pthread_mutex_lock(&policy_lock);
    free_policy();
    dbus_broker_policy = build_policy(RULES_FILENAME);
    pthread_mutex_unlock(&policy_lock);
}

void sigint_handler(int signal)
{
    DBUS_BROKER_WARNING("<received signal interrupt> %s", "");
    free_dlinks();
    free_policy();
    lws_ring_destroy(ring);
    exit(0);
}

void sighup_handler(int signal)
{
    pthread_t reload_thr;
    pthread_create(&reload_thr, NULL, (void *(*)(void *)) reload_policy, NULL);
    DBUS_BROKER_EVENT("Re-loading policy %s", "");
}

/*
 * Cycle through linked-list of current dbus-signals being subscribed to.
 * If there are any signals queued up, start the exchange of communication.
 */
static inline void service_signals(void)
{
    struct dbus_link *curr = dlinks;

    // Make check on the status of the connection
    // add-link remove-link functions

    while (curr) { 
        dbus_connection_read_write(curr->dconn, 0);
        DBusMessage *msg = dbus_connection_pop_message(curr->dconn);
        curr = curr->next;

        if (!msg)
            continue;
    
        if (dbus_message_get_type(msg) != DBUS_MESSAGE_TYPE_SIGNAL) {
            dbus_message_unref(msg);
            continue;
        }

        struct json_response *jrsp = init_jrsp();
        jrsp->response_to[0] = '\0';
        snprintf(jrsp->type, JSON_REQ_ID_MAX - 1, "%s", JSON_SIG);

        load_json_response(msg, jrsp);

        jrsp->interface = dbus_message_get_interface(msg);
        jrsp->member = dbus_message_get_member(msg);
        jrsp->path = dbus_message_get_path(msg);
        char *reply = prepare_json_reply(jrsp);

        if (!reply) 
            goto free_msg;

        pthread_mutex_lock(&ring_lock);
        lws_ring_insert(ring, reply, 1);
        free(reply);
        lws_callback_on_writable(curr->wsi);

        pthread_mutex_unlock(&ring_lock);

free_msg:
        dbus_message_unref(msg);
        free(jrsp);
    }
}

static void run(struct dbus_broker_args *args)
{
    srand48(time(NULL));

    dbus_broker_policy = build_policy(args->rule_file);

    struct etc_policy etc = dbus_broker_policy->etc;

    if (pthread_mutex_init(&ring_lock, NULL) < 0)
        DBUS_BROKER_ERROR("initializing ring-lock");

    if (pthread_mutex_init(&policy_lock, NULL) < 0)
        DBUS_BROKER_ERROR("initializing policy-lock");

    struct dbus_broker_server *server = start_server(BROKER_DEFAULT_PORT);
    DBUS_BROKER_EVENT("<Server has started listening> [Port: %d]",
                        BROKER_DEFAULT_PORT);

    int default_socket = server->dbus_socket;

    fd_set server_set;

    struct lws_context *ws_context = NULL;
    ws_context = create_ws_context(BROKER_UI_PORT);

    if (!ws_context)
        DBUS_BROKER_ERROR("WebSockets-Server");

    DBUS_BROKER_EVENT("<WebSockets-Server has started listening> [Port: %d]",
                        BROKER_UI_PORT);

    dbus_broker_running = 1;
    dlinks = NULL;

    while (dbus_broker_running) {

        FD_ZERO(&server_set);
        FD_SET(default_socket, &server_set);

        struct timeval tv = { .tv_sec=0, .tv_usec=DBUS_BROKER_TIMEOUT };
        // Poll on port-5555
        int ret = select(default_socket + 1, &server_set, NULL, NULL, &tv);

        if (ret > 0) {
            int client = v4v_accept(default_socket, &server->peer);
            DBUS_BROKER_EVENT("<Client has made a connection> [Dom: %d Client: %d]",
                                server->peer.domain, client);
            init_request(client);
        }

        // check signal subscriptions
        service_signals();
        // websocket servicing event-loop
        lws_service(ws_context, WS_LOOP_TIMEOUT);
    }

    free(server);
    free_policy();
    close(default_socket);

    lws_ring_destroy(ring);
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
    verbose_logging = false;

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
                verbose_logging = true;
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
        .verbose=verbose_logging,
        .bus_name=bus_file,
        .logging_file=logging_file,
        .rule_file=rule_file,
    };

    struct sigaction sa = { .sa_handler=sigint_handler };

    if (sigaction(SIGINT, &sa, NULL) < 0)
        DBUS_BROKER_ERROR("sigaction");

    run(&args);

    return 0;
}

