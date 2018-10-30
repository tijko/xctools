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
int broker_message(int client, int domid)
{
    fd_set ex_set;

    int srv = connect_to_system_bus();
    int bytes = 0;

    do {
        FD_ZERO(&ex_set);
        FD_SET(client, &ex_set);
        FD_SET(srv, &ex_set);

        struct timeval tv = { .tv_sec=2, .tv_usec=0 };
        // Poll on read-ready
        int ret = select(srv + 1, &ex_set, NULL, NULL, &tv);

        if (ret == 0)
            continue;
        if (ret < 0)
            return ret;

        // Depending on which fd is ready to be read, determines which
        // function pointer to pass to `exchange` 
        if (FD_ISSET(srv, &ex_set))
            bytes = exchange(srv, client, recv, v4v_send, domid);
        else
            bytes = exchange(client, srv, v4v_recv, send, domid);

    } while (bytes > 0);

    close(srv);

    return 0;
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

void print_usage(void)
{
    printf("rpc-broker <flag> <argument>\n");
    printf("\t-b  [--bus-name=BUS]                    ");
    printf("A dbus bus name to make the connection to.\n");
    printf("\t-h  [--help]                            ");
    printf("Prints this usage description.\n");
    printf("\t-l  [--logging[=optional FILENAME]      ");
    printf("Enables logging to a default path, optionally set.\n");
    printf("\t-p  [--policy-file=FILENAME]            ");
    printf("Provide a policy file to run against.\n");
    printf("\t-r  [--raw-dbus=PORT]                   ");
    printf("Sets rpc-broker to run on given port as raw DBus.\n");
    printf("\t-v  [--verbose]                         ");
    printf("Adds extra information (run with logging).\n");
    printf("\t-w  [--websockets=PORT]                 ");
    printf("Sets rpc-broker to run on given address/port as websockets.\n");
}

void sigint_handler(int signal)
{
    DBUS_BROKER_WARNING("<received signal interrupt> %s", "");
    free_dlinks();
    free_policy();

    if (ring)
        lws_ring_destroy(ring);

    exit(0);
}

void sighup_handler(int signal)
{
    reload_policy = true;
    DBUS_BROKER_EVENT("Re-loading policy %s", "");
}

/*
 * Cycle through linked-list of current dbus-signals being subscribed to.
 * If there are any signals queued up, start the exchange of communication.
 */
static inline void service_dbus_signals(void)
{
    struct dbus_link *curr = dlinks;

    while (curr) { 

        DBusMessage *msg = NULL;

        if (curr->dconn && dbus_connection_get_is_connected(curr->dconn)) {
            dbus_connection_read_write(curr->dconn, 0);
            msg = dbus_connection_pop_message(curr->dconn);
        }

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

        lws_ring_insert(ring, reply, 1);
        free(reply);
        lws_callback_on_writable(curr->wsi);

free_msg:
        dbus_message_unref(msg);
        free(jrsp);
    }
}

static void load_policy(const char *policy_file)
{
    srand48(time(NULL));
    dbus_broker_policy = build_policy(policy_file);
    if (pthread_mutex_init(&policy_lock, NULL) < 0)
        DBUS_BROKER_ERROR("initializing policy-lock");
}

static void run_websockets(struct dbus_broker_args *args)
{
    struct lws_context *ws_context = NULL;
    ws_context = create_ws_context(BROKER_UI_PORT);

    if (!ws_context)
        DBUS_BROKER_ERROR("WebSockets-Server");

    DBUS_BROKER_EVENT("<WebSockets-Server has started listening> [Port: %d]",
                        BROKER_UI_PORT);

    load_policy(args->rule_file);

    while (dbus_broker_running) {

        service_dbus_signals(); 
        lws_service(ws_context, WS_LOOP_TIMEOUT);

        if (reload_policy) {
            free_policy();
            dbus_broker_policy = build_policy(RULES_FILENAME);
            reload_policy = false;
        }
    }

    lws_ring_destroy(ring);
    lws_context_destroy(ws_context);
}

static int loop(int rsock, int ssock,
                ssize_t (*rcv)(int, void *, size_t, int),
                ssize_t (*snd)(int, const void *, size_t, int))
{
    int total = 0;
    
    char buf[8192];

    while (1) {

        int ret = rcv(rsock, buf, 8192, 0);
        if (ret < 0)
            break;
        total += ret; 

        snd(ssock, buf, ret, 0);
    }

    return total;
}

void run_rawdbus(struct dbus_broker_args *args)
{
    struct dbus_broker_server *server = start_server(args->port);
    DBUS_BROKER_EVENT("<Server has started listening> [Port: %d]", args->port);

    int default_socket = server->dbus_socket;

    fd_set server_set;
    load_policy(args->rule_file);

    while (dbus_broker_running) {

        FD_ZERO(&server_set);
        FD_SET(default_socket, &server_set);

        struct timeval tv = { .tv_sec=1, .tv_usec=0 };//DBUS_BROKER_TIMEOUT };
        int ret = select(default_socket + 1, &server_set, NULL, NULL, &tv);
        if (ret > 0) {

            int client = v4v_accept(default_socket, &server->peer);
            DBUS_BROKER_EVENT("<Client has made a connection> [Dom: %d Client: %d]",
                                server->peer.domain, client);
            v4v_addr_t client_addr = { .domain=0, .port=0 };
            
            if (v4v_getpeername(client, &client_addr) < 0) 
                DBUS_BROKER_WARNING("getpeername call failed <%s>", strerror(errno));
            else {
                // while both loops finish...
                int srv = connect_to_system_bus();
                fcntl(srv, F_SETFL, O_NONBLOCK);
                fcntl(client, F_SETFL, O_NONBLOCK);
                int sret = 1, cret = 1;
                while ( 1 ) {
//                while (sret >= 0 || cret >= 0) {
                    // client recv loop
                    cret = loop(client, srv, v4v_recv, send);
                    // server recv loop
                    sret = loop(srv, client, recv, v4v_send);
                }
                //broker_message(client, client_addr.domain); 
            }
        }

        if (reload_policy) {
            free_policy();
            dbus_broker_policy = build_policy(RULES_FILENAME);
            reload_policy = false;
        }
    }

    free(server);
    close(default_socket);
}

int main(int argc, char *argv[])
{
    const char *dbus_broker_opt_str = "b:hl::p:r:vw:";

    struct option dbus_broker_opts[] = {
        { "bus-name",    required_argument,   0, 'b' },
        { "help",        no_argument,         0, 'h' },
        { "logging",     optional_argument,   0, 'l' },
        { "policy-file", required_argument,   0, 'p' },
        { "raw-dbus",    required_argument,   0, 'r' },
        { "verbose",     no_argument,         0, 'v' },
        { "websockets",  required_argument,   0, 'w' }, 
        {  0,            0,        0,         0      }
    };

    int opt;
    int option_index;

    uint32_t port;

    bool logging = false;
    verbose_logging = false;
    void (*mainloop)(struct dbus_broker_args *args);

    char *bus_file = NULL;
    char *raw_dbus = NULL;
    char *websockets = NULL;
    char *logging_file = "";
    char *policy_file  = RULES_FILENAME;

    bool proto = false;

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

            case ('p'):
                policy_file = optarg;
                break;

            case ('r'):
                if (proto)
                    goto conn_type_error; 
                raw_dbus = optarg;
                proto = true;
                break;

            case ('v'):
                verbose_logging = true;
                break;

            case ('w'):
                if (proto)
                    goto conn_type_error; 
                websockets = optarg;
                proto = true;
                break;

            case ('h'):
            case ('?'):
                print_usage();
                exit(0);
                break;
        }
    }

    errno = 0;
    if (raw_dbus) {
        port = strtol(raw_dbus, NULL, 0);
        if (errno != 0)
            DBUS_BROKER_ERROR("Invalid raw-dbus port");
        mainloop = run_rawdbus;
    } else if (websockets) {
        port = strtol(websockets, NULL, 0);
        if (errno != 0)
            DBUS_BROKER_ERROR("Invalid websockets port");
        mainloop = run_websockets;
    } else
        goto conn_type_error;

    struct dbus_broker_args args = {
        .logging=logging,
        .verbose=verbose_logging,
        .bus_name=bus_file,
        .logging_file=logging_file,
        .rule_file=policy_file,
        .port=port,
    };

    struct sigaction sa = { .sa_handler=sigint_handler };

    if (sigaction(SIGINT, &sa, NULL) < 0)
        DBUS_BROKER_ERROR("sigaction");

    dbus_broker_running = 1;
    dlinks = NULL;
    ring = NULL;
    reload_policy = false;

    mainloop(&args);

    free_policy();
    free_dlinks();

    return 0;

conn_type_error:

    printf("Must supply at least one (and no more than one) connection type.\n");
    print_usage();

    return 0;
}

