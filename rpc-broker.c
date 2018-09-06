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
        int ret = select(srv + 1, &ex_set, NULL, NULL, &tv);

        if (ret == 0)
            continue;
        if (ret < 0)
            DBUS_BROKER_ERROR("select");

        if (FD_ISSET(srv, &ex_set)) 
            bytes = exchange(srv, client, recv, v4v_send, req);
        else
            bytes = exchange(client, srv, v4v_recv, send, req);

    } while (bytes > 0);

    close(srv);

    // set errno in void *
    return NULL;
}

signed int is_stubdom(uint16_t domid)
{
    struct xs_handle *xsh = xs_open(XS_OPEN_READONLY);
    size_t len = 0;

    if (!xsh) 
        return -1;

    char *path = xs_get_domain_path(xsh, domid);
    path = realloc(path, sizeof(char) * strlen(path) + 8); 
    strcat(path, "/target");

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

    v4v_addr_t client_addr;

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
    sem_wait(&memory_lock);
    free_policy();
    dbus_broker_policy = build_policy(RULES_FILENAME);
    sem_post(&memory_lock); 
}

void sigint_handler(int signal)
{
    DBUS_BROKER_WARNING("<received signal interrupt> %s", "");
    exit(0);
}

void sighup_handler(int signal)
{
    pthread_t reload_thr;
    pthread_create(&reload_thr, NULL, (void *(*)(void *)) reload_policy, NULL);
    DBUS_BROKER_EVENT("Re-loading policy %s", "");
}

static void run(struct dbus_broker_args *args)
{
    srand48(time(NULL));

    for (;db_list() == NULL;);
       
    dbus_broker_policy = build_policy(args->rule_file);

    struct etc_policy etc = dbus_broker_policy->etc;

    sem_init(&memory_lock, 0, 1);

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

    dbus_broker_running = 1;
    connection_open = 1;

    while (dbus_broker_running) {

        FD_ZERO(&server_set);
        FD_SET(default_socket, &server_set);
//        lws_service(ws_context, WS_LOOP_TIMEOUT);

        struct timeval tv = { .tv_sec=0, .tv_usec=DBUS_BROKER_TIMEOUT };
        int ret = select(default_socket + 1, &server_set, NULL, NULL, &tv);

//        if (ret == 0)
//            continue;

//        if (ret < 0)
//            DBUS_BROKER_ERROR("select");

//        if (FD_ISSET(default_socket, &server_set) == 0)
//            continue;
  
        if (FD_ISSET(default_socket, &server_set) > 0) {
            int client = v4v_accept(default_socket, &server->peer);

            if (client < 0)
                DBUS_BROKER_ERROR("v4v_accept");

            DBUS_BROKER_EVENT("<Client has made a connection> [Dom: %d Client: %d]",
                                server->peer.domain, client);

            struct dbus_request *dreq = malloc(sizeof *dreq);
            dreq->client = client;

            v4v_addr_t client_addr;

            v4v_getpeername(client, &client_addr);
            dreq->domid = client_addr.domain;

            broker_messaged(req);
//            init_request(client);
        }

        lws_service(ws_context, WS_LOOP_TIMEOUT);
    }

    free(server);
    free(dbus_broker_policy);
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

    struct sigaction sa = { .sa_handler=sigint_handler };

    if (sigaction(SIGINT, &sa, NULL) < 0)
        DBUS_BROKER_ERROR("sigaction");

    run(&args);

    return 0;
}

