#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#include "options.h"

void print_usage(void)
{
    printf("rpc-broker\n");
    printf("\t-b  [--bus-name=BUS]                A dbus bus name to make the connection to.\n");
    printf("\t-h  [--help]                        Prints this usage description.\n");
    printf("\t-l  [--logging[=optional FILENAME]  Enables logging to a default path, optionally set.\n");
    printf("\t-p  [--policy-file=FILENAME]        Provide a policy file to run against.\n");
    printf("\t-s  [--server-path=FILENAME]        The server path address to bind on.\n");
    printf("\t-v  [--verbose]                     Adds extra information (run with logging).\n"); 
}

void rpc_broker_sigint(int signal)
{
    unlink(BROKER_SERVER_PATH); // handle other paths
}

struct rpc_broker_server *start_server(char *server_path)
{
    struct rpc_broker_server *server = malloc(sizeof *server);
    server->server_path = server_path;

    server->rpc_socket = socket(AF_UNIX, SOCK_STREAM, 0);
    if (server->rpc_socket < 0)
        ERROR(strerror(errno));

    server->addr.sun_family = AF_UNIX;

    size_t path_length = strlen(server_path) + 1;

    if (snprintf(server->addr.sun_path, path_length, server_path) < 0)
        ERROR(strerror(errno));

    if (bind(server->rpc_socket, (struct sockaddr *) &server->addr, 
                           sizeof(struct sockaddr_un)) < 0)
        ERROR(strerror(errno));

    if (listen(server->rpc_socket, 1) < 0)
        ERROR(strerror(errno));

    return server;
}

void run_rpc_broker(struct rpc_broker_args *args)
{
    char *policy = get_policy(args->policy_file);

    if (policy == NULL)
        ERROR(strerror(errno));

    struct rpc_broker_server *server = start_server(args->server_path);

    int rpc_socket = server->rpc_socket;
    struct sockaddr_un addr = server->addr;
    socklen_t client_length;

    while ( 1 ) {

        int client = accept(rpc_socket, (struct sockaddr *) &addr, &client_length);

        if (client < 0)
            ERROR(strerror(errno));

        if (!client_allowed(client, policy)) // log; client-recv_req
            continue;

        if (make_dbus_req(client) < 0) // log
            printf("DBus request error!\n");
    }
}

int make_dbus_req(int client)
{
    pthread_t dbus_req_thread;

    char req[CLIENT_DBUS_REQ_LEN];
    int rbytes = recv(client, req, CLIENT_DBUS_REQ_LEN - 1, MSG_WAITALL);

    if (rbytes < 0)
        return rbytes;

    // strtok into parts, run check if formed correctly
    struct rpc_broker_dbus_req dbus_req = { .dest="org.freedesktop.DBus",
                                            .path="/org/freedesktop/DBus",
                                            .iface="org.freedesktop.DBus",
                                            .call="ListNames", 
                                            .client=client };

    int ret = pthread_create(&dbus_req_thread, NULL, 
                            (void *(*)(void *)) dbus_request, (void *) &dbus_req);

    return ret;
}

void *dbus_request(void *req)
{
    DBusError error;
    dbus_error_init(&error);

    DBusConnection *conn = dbus_bus_get(DBUS_BUS_SYSTEM, &error);

    struct rpc_broker_dbus_req *dbus_req = (struct rpc_broker_dbus_req *) req;

    if (!conn) // log
        return NULL;

    DBusPendingCall *pc = NULL;
    DBusMessage *message = dbus_message_new_method_call(dbus_req->dest, 
                                                        dbus_req->path,
                                                        dbus_req->iface,
                                                        dbus_req->call);
    if (!message) // log
        return NULL;

    if (!dbus_connection_send_with_reply(conn, message, &pc, 
                                         DBUS_REQUEST_TIMEOUT))
        return NULL;

    dbus_connection_flush(conn);
    dbus_pending_call_block(pc);
    dbus_connection_flush(conn);
    
    dbus_message_unref(message);
    message = dbus_pending_call_steal_reply(pc);

    DBusMessageIter iter, sub;
    dbus_message_iter_init(message, &iter);
    dbus_message_iter_recurse(&iter, &sub);

    int client = dbus_req->client;

    do {
        char *dbus_var;
        // check type
        dbus_message_iter_get_basic(&sub, &dbus_var);
        int wbytes = send(client, dbus_var, strlen(dbus_var), MSG_DONTWAIT);
        printf("%d bytes sent\n", wbytes);
    } while (dbus_message_iter_next(&sub));

    return NULL;
}

char *get_policy(char *policy_file)
{
    struct stat policy_stat;

    if (stat(policy_file, &policy_stat) < 0)
        return NULL;

    size_t policy_size = policy_stat.st_size;
    
    char *policy = malloc(sizeof(char) * policy_size + 1);

    int policy_fd = open(policy_file, O_RDONLY);

    if (policy_fd < 0)
        return NULL;

    int rbytes = read(policy_fd, policy, policy_size);

    if (rbytes < 0)
        return NULL;

    policy[rbytes] = '\0';
    close(policy_fd);

    return policy;
}

int client_allowed(int client, char *policy)
{
    char client_req[CLIENT_REQ_LEN];

    int rbytes = recv(client, client_req, CLIENT_REQ_LEN - 1, MSG_WAITALL); 

    if (rbytes < 0)
        ERROR(strerror(errno));

    client_req[rbytes] = '\0';

	return 1;
//    return strstr(policy, client_req) ? 1 : 0;
}

int main(int argc, char *argv[])
{
    const char *rpc_broker_opt_str = "b:hl::p:s:v";

    struct option rpc_broker_opts[] = {
        { "bus-name",    required_argument, 0, 'b' },
        { "help",        no_argument,       0, 'h' },
        { "logging",     optional_argument, 0, 'l' },
        { "policy-file", required_argument, 0, 'p' },
        { "server-path", required_argument, 0, 's' },
        { "verbose",     no_argument,       0, 'v' },
        {  0,           0,       0,             0  }
    };

    int opt;
    int option_index;

    bool logging = false;
    bool verbose = false;

    char *bus_file = NULL;
    char *logging_file = BROKER_LOG;
    char *policy_file  = BROKER_POLICY;
    char *server_path  = BROKER_SERVER_PATH;

    while ((opt = getopt_long(argc, argv, rpc_broker_opt_str,
                              rpc_broker_opts, &option_index)) != -1) {

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

            case ('s'):
                server_path = optarg;
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

    struct rpc_broker_args args = {
        .logging=logging,
        .verbose=verbose,
        .bus_name=bus_file,
        .logging_file=logging_file,
        .policy_file=policy_file,
        .server_path=server_path
    };

    struct sigaction rpc_sa = { .sa_handler=rpc_broker_sigint };
    if (sigaction(SIGINT, &rpc_sa, NULL) < 0)
        ERROR(strerror(errno));

    run_rpc_broker(&args);

    return 0;
}

