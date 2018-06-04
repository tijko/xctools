#include <dbus/dbus.h>
#include <stdlib.h>


#define ERROR(msg)                      \
    do {                                \
        printf("Error: %s\n", msg);     \
        exit(0);                        \
    } while ( 1 )                       \


#define BROKER_LOG         "~/.broker_log"
#define BROKER_POLICY      "/etc/broker_policy"
#define BROKER_SERVER_PATH "/tmp/broker"

#define DBUS_REQUEST_TIMEOUT 1000

#define CLIENT_REQ_LEN 16
#define CLIENT_DBUS_REQ_LEN 256

struct rpc_broker_server {
    char *server_path;
    int rpc_socket;
    struct sockaddr_un addr;
};

struct rpc_broker_dbus_session {
    DBusConnection *conn;
    DBusMessage *msg;
    DBusMessageIter *msg_iter;
    DBusPendingCall *pc;
};

struct rpc_broker_dbus_req {
    char *dest;
    char *path;
    char *iface;
    char *call;
    int client;
};

struct rpc_broker_args {
    bool logging;
    bool verbose;
    char *bus_name;
    char *logging_file;
    char *policy_file;
    char *server_path;
};

void *dbus_request(void *req);
char *get_policy(char *policy_file);
void rpc_broker_sigint(int signal);
int client_allowed(int client, char *policy);
int make_dbus_req(int client);
struct rpc_broker_server *start_server(char *server_path);
void run_rpc_broker(struct rpc_broker_args *args);
