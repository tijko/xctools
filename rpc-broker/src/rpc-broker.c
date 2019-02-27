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
#include <getopt.h>
#include <signal.h>
#include <sys/time.h>
#include <unistd.h>

#include "rpc-broker.h"


uv_loop_t *rawdbus_loop;
bool reload_policy;

static int broker_message(struct raw_dbus_conn *conn)
{
    int domid;
    int ret, total;

    bool is_client;
    int receiver, sender;

    total = 0;
    domid = conn->client_domain;
    is_client = conn->is_client;
    receiver = conn->receiver;
    sender = conn->sender;
    while ((ret = exchange(receiver, sender, domid, is_client)) != 0) {
        total += ret;
    }

    return total;
}

signed int is_stubdom(uint16_t domid)
{
    size_t len;
    len = 0;
#ifdef HAVE_XENSTORE
    struct xs_handle *xsh;
    char *path;
    void *ret;

    xsh = xs_open(XS_OPEN_READONLY);

    if (!xsh)
        return -1;

    path = xs_get_domain_path(xsh, domid);
    path = realloc(path, strlen(path) + XENSTORE_TARGET_LEN);
    strcat(path, XENSTORE_TARGET);

    ret = xs_read(xsh, XBT_NULL, path, &len);

    if (ret)
       free(ret);

    free(path);
    xs_close(xsh);
#endif
    return len;
}

int get_domid(int client)
{
    int domain;

    domain = 0;
    /*
     * When using rpc-broker over V4V, we want to be able to
     * firewall against domids. The V4V interposer stores the
     * domid as follows:
     * sin.sin_addr.s_addr = htonl ((uint32_t) peer->domain | 0x1000000);
     * If we're not using V4V, just return 0 for the domid.
     */
#ifdef HAVE_V4V
    struct sockaddr_in client_addr;
    socklen_t client_addr_len;

    client_addr_len = sizeof(client_addr);

    if (getpeername(client, &client_addr, &client_addr_len) < 0)
        DBUS_BROKER_WARNING("getpeername call failed <%s>", strerror(errno));
    else
        domain = ntohl(client_addr.sin_addr.s_addr) & ~0x1000000;
#endif

    return domain;
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

    if (rawdbus_loop) {
        uv_stop(rawdbus_loop);
        uv_loop_close(rawdbus_loop);
        free(rawdbus_loop);
    }

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
static void service_ws_signals(void)
{
    bool remove_link;
    struct dbus_link *curr;
    DBusMessage *msg;
    struct json_response *jrsp;
    char *reply;

    if (!dlinks)
        return;

    curr = dlinks;
    remove_link = false;

    do {
        msg = NULL;

        if (curr->dconn && dbus_connection_get_is_connected(curr->dconn)) {
            dbus_connection_read_write(curr->dconn, 0);
            msg = dbus_connection_pop_message(curr->dconn);
        } else
            remove_link = true;

        if (!msg)
            goto next_link;

        if (dbus_message_get_type(msg) != DBUS_MESSAGE_TYPE_SIGNAL)
            goto unref_msg;

        jrsp = init_jrsp();
        jrsp->response_to[0] = '\0';
        snprintf(jrsp->type, JSON_REQ_ID_MAX - 1, "%s", JSON_SIG);

        load_json_response(msg, jrsp);

        jrsp->interface = dbus_message_get_interface(msg);
        jrsp->member = dbus_message_get_member(msg);
        jrsp->path = dbus_message_get_path(msg);

        reply = prepare_json_reply(jrsp);

        if (!reply)
            goto free_msg;

        lws_callback_on_writable(curr->wsi);
        lws_ring_insert(ring, reply, 1);
        free(reply);

free_msg:
        free(jrsp);

unref_msg:
        dbus_message_unref(msg);

next_link:
        curr = curr->next;

        if (remove_link) {
            remove_dlink(curr->prev);
            remove_link = false;
            signal_subscribers--;
        }

    } while (curr != dlinks);
}

static void run_websockets(struct dbus_broker_args *args)
{
    struct lws_context *ws_context;

    ws_context = NULL;
    signal_subscribers = 0;
    if ((ws_context = create_ws_context(args->port)) == NULL)
        DBUS_BROKER_ERROR("WebSockets-Server");

    dbus_broker_policy = build_policy(args->rule_file);

    DBUS_BROKER_EVENT("<WebSockets-Server has started listening> [Port: %d]",
                        args->port);

    while (dbus_broker_running) {

        lws_service(ws_context, WS_LOOP_TIMEOUT);
        service_ws_signals();

        if (reload_policy) {
            free_policy();
            dbus_broker_policy = build_policy(args->rule_file);
            reload_policy = false;
        }
    }

    if (ring)
        lws_ring_destroy(ring);

    if (ws_context)
        lws_context_destroy(ws_context);
}

static void close_client_rawdbus(uv_handle_t *handle)
{
    struct raw_dbus_conn *conn;

    conn = (struct raw_dbus_conn *) handle->data;
    close(conn->receiver);
    uv_unref(handle);

    if (conn)
        free(conn);

    conn = NULL;
}

static void close_server_rawdbus(uv_handle_t *handle)
{
    struct dbus_broker_server *server;
    server = (struct dbus_broker_server *) handle->data;
    close(server->dbus_socket);
    uv_unref(handle);
}

static void service_rdconn_cb(uv_poll_t *handle, int status, int events)
{
    int ret;
    struct raw_dbus_conn *conn;

    conn = (struct raw_dbus_conn *) handle->data;
    ret = 0;

    if (events & UV_READABLE) {
        ret = broker_message(conn);
        if (ret <= 0)
            uv_close((uv_handle_t *) handle, close_client_rawdbus);
    }
}

static void init_rawdbus_conn(uv_loop_t *rawdbus_loop, int sender,
                              int receiver, int domain, bool is_client)
{
    struct raw_dbus_conn *conn;

    conn = malloc(sizeof *conn);
    if (!conn)
        DBUS_BROKER_ERROR("Malloc Failed!");

    conn->sender = sender;
    conn->receiver = receiver;
    conn->client_domain = domain;
    conn->is_client = is_client;
    conn->handle.data = conn;
    uv_poll_init(rawdbus_loop, &conn->handle, conn->receiver);
    uv_poll_start(&conn->handle, UV_READABLE | UV_DISCONNECT,
                   service_rdconn_cb);
}

static void service_rawdbus_server(uv_poll_t *handle, int status, int events)
{
    struct dbus_broker_server *dbus_server;
    uv_loop_t *loop;
    int client, server, domain;

    dbus_server= (struct dbus_broker_server *) handle->data;
    loop = dbus_server->mainloop;

    if (events & UV_READABLE) {
	        socklen_t clilen = sizeof(dbus_server->peer);
	        client = accept(dbus_server->dbus_socket,
                           (struct sockaddr *) &dbus_server->peer, &clilen);
            server = connect_to_system_bus();
            domain = get_domid(client);
            init_rawdbus_conn(loop, server, client, domain, true);
            init_rawdbus_conn(loop, client, server, domain, false);

    } else if (events & UV_DISCONNECT) {
        dbus_broker_running = 0;
        uv_close((uv_handle_t *) handle, close_server_rawdbus);
    }
}

static void run_rawdbus(struct dbus_broker_args *args)
{
    struct dbus_broker_server server;
    if (start_server(&server, args->port) < 0)
        DBUS_BROKER_ERROR("DBus server failed to start!");

    DBUS_BROKER_EVENT("<Server has started listening> [Port: %d]", args->port);

    dbus_broker_policy = build_policy(args->rule_file);

    rawdbus_loop = malloc(sizeof *rawdbus_loop);
    if (!rawdbus_loop)
        DBUS_BROKER_ERROR("Malloc Failed!");

    uv_loop_init(rawdbus_loop);

    uv_poll_init(rawdbus_loop, &server.handle, server.dbus_socket);
    uv_poll_start(&server.handle, UV_READABLE | UV_DISCONNECT,
                   service_rawdbus_server);

    server.mainloop = rawdbus_loop;
    server.port = args->port;
    server.handle.data = &server;

    while (dbus_broker_running) {

        uv_run(rawdbus_loop, UV_RUN_DEFAULT);

        if (reload_policy) {
            free_policy();
            dbus_broker_policy = build_policy(args->rule_file);
            reload_policy = false;
        }
    }

    uv_stop(rawdbus_loop);
    uv_loop_close(rawdbus_loop);
    free(rawdbus_loop);
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

    int opt, option_index;
    void (*mainloop)(struct dbus_broker_args *args);

    char *websockets, *raw_dbus;
    char *logging_file, *bus_file, *policy_file;
    uint32_t port;
    bool proto, logging;

    logging = false;
    verbose_logging = false;

    bus_file = NULL;
    raw_dbus = NULL;
    websockets = NULL;
    logging_file = "";
    policy_file  = RULES_FILENAME;

    proto = false;

    dbus_broker_opt_str = "b:hl::p:r:vw:";

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
    rawdbus_loop = NULL;
    ring = NULL;
    reload_policy = false;
    CACHE_INIT(domain_uuids, UUID_CACHE_LIMIT);
    mainloop(&args);

    free_policy();
    free_dlinks();
    free_uuids();

    return 0;

conn_type_error:

    printf("Must supply at least one (and no more than one) connection type.\n");
    print_usage();

    return 0;
}

