#define _GNU_SOURCE
#include <ctype.h>
#include <dbus/dbus.h>
#include <json.h>
#include <libwebsockets.h>
#include <libv4v.h>
#include <libxml/parser.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <xenstore.h>


#define DBUS_BROKER_ERROR(call)                                       \
    do {                                                              \
        openlog(NULL, LOG_PERROR, 0);                                 \
        syslog(LOG_ERR, "%s %s %s", call, __func__, strerror(errno)); \
        closelog();                                                   \
        exit(0);                                                      \
    } while ( 1 )                                                     \

#define DBUS_LOG(type, fmt, ...)                                      \
    ({                                                                \
        char *buf;                                                    \
        asprintf(&buf, fmt, __VA_ARGS__);                             \
        syslog(type, "%s", buf);                                      \
        free(buf);                                                    \
    })                                                                \
    
#define DBUS_BROKER_WARNING(fmt, ...) DBUS_LOG(LOG_WARNING, fmt, __VA_ARGS__)
#define DBUS_BROKER_EVENT(fmt, ...)   DBUS_LOG(LOG_INFO, fmt, __VA_ARGS__)

#define DBUS_REQ_ARG(buf, fmt, ...) ({ asprintf(&buf, fmt, __VA_ARGS__); })

#define RULES_FILENAME "/etc/rpc-broker.rules"
#define RULES_MAX_LENGTH 256
#define RULE_MAX_LENGTH  256

struct rule {
    uint8_t policy:1;
    uint8_t stubdom:1;
    uint8_t if_bool_flag:1;
    const char *destination;
    const char *path;
    const char *interface;
    const char *member;
    const char *if_bool;
    const char *domtype;
    const char *rule_string;
};

#define MAX_RULES      512 
#define MAX_DOMAINS    128
#define ETC_MAX_FILE 0xffff

struct etc_policy {
    const char *filename;
    const char *filepath;
    const size_t filesize;
    char etc_file[ETC_MAX_FILE];
    uid_t  file_owner;
    gid_t  file_group;
    mode_t file_mode;
    time_t last_access;
    time_t last_modify;
    // file-hash?
    size_t count;
    struct rule rules[MAX_RULES];
};

struct domain_policy {
    uint16_t domid;
    size_t count;
    char *uuid;
    struct rule rules[MAX_RULES];
};

struct policy {
    size_t domain_number;
    time_t policy_load_time;
    size_t allowed_requests;
    size_t denied_requests;
    size_t total_requests;
    struct etc_policy etc;
    struct domain_policy domains[MAX_DOMAINS];
};

struct policy *dbus_broker_policy;

// DBus-Broker server
#define BROKER_DEFAULT_PORT 5555
#define BROKER_UI_PORT      8080
        
struct dbus_broker_server {
    int dbus_socket;
    v4v_addr_t addr;
    v4v_addr_t peer;
};

#define WS_LOOP_TIMEOUT     200   // length of time each service of the websocket 
                                  // event-loop (millisecs)
#define DBUS_BROKER_TIMEOUT 100

// DBus-Broker messages
#define DBUS_REQ_TIMEOUT 1000
#define DBUS_MSG_LEN     8192
#define DBUS_ARG_LEN     1024

#define DBUS_BUS_ADDR_LEN 256
#define DBUS_MAX_ARG_LEN   16

struct dbus_message {
    const char *destination;
    const char *interface;
    const char *path;
    const char *member;
    // struct argument {
    //   void *data;
    //   char dbus_type;
    //   char actual_type;
    // };
    uint8_t arg_number;
    char arg_sig[DBUS_MAX_ARG_LEN];
    char json_sig[DBUS_MAX_ARG_LEN];
    void *args[DBUS_MAX_ARG_LEN];
};

#define DBUS_READ "read"
#define DBUS_LIST "list"

#define CLIENT_REQ_LEN       16
#define CLIENT_DBUS_REQ_LEN 256

#define VM_UUID_LEN   33
#define DOMID_UUID_LEN 8
#define DOMID_SECTION 24

#define WS_RING_BUFFER_MEMBER_SIZE 4096
#define WS_RING_BUFFER_MEMBER_NUM    64

#define WS_USER_MEM_SIZE 8192  // the amount memory that is allocated for user
                               // for each ws-callback
 
struct json_request {
    uint32_t id;
    DBusConnection *conn;
    struct lws *wsi;
    struct dbus_message dmsg;
};

#define JSON_REQ_ID_MAX 16
#define JSON_TYPE_MAX   16
#define JSON_REQ_MAX   256

struct json_response {
    uint32_t id;
    const char *path;
    const char *interface;
    const char *member;
    char response_to[JSON_REQ_ID_MAX];
    char type[JSON_TYPE_MAX]; 
    char arg_sig[DBUS_MAX_ARG_LEN];
    struct json_object *args;
};

static const xmlChar XML_NAME_PROPERTY[]      = "name";
static const xmlChar XML_DIRECTION_PROPERTY[] = "direction";

static const char XML_IN_FIELD[]           = "in";
static const char XML_TYPE_FIELD[]         = "type";

struct lws_ring *ring;
sem_t memory_lock;
int dbus_broker_running;
int connection_open;

struct broker_signal {
    DBusConnection *conn;
    struct lws *wsi;
    // track original signal-call
    // give timestamp too?
};

struct dbus_request {
    uint16_t domid;
    uint16_t client;
};

struct dbus_broker_args {
    bool logging;
    bool verbose;
    const char *bus_name;
    const char *logging_file;
    const char *rule_file;
};

// rpc-broker.c
void *broker_message(void *request);

int init_request(int client);

signed int is_stubdom(uint16_t domid);

void print_usage(void);

void sigint_handler(int signal);


// src/dbus.c
DBusConnection *create_dbus_connection(void);

struct dbus_broker_server *start_server(int port);

void dbus_default(struct dbus_message *dmsg);

int connect_to_system_bus(void);

void *dbus_signal(void *subscriber);

signed int convert_raw_dbus(struct dbus_message *dmsg, 
                            const char *msg, size_t len);

DBusMessage *make_dbus_call(DBusConnection *conn, struct dbus_message *dmsg);

char *db_query(DBusConnection *conn, char *arg);

char *dbus_introspect(struct json_request *jreq);

#define DBUS_DB_DEST     "com.citrix.xenclient.db"
#define DBUS_DB_IFACE    "com.citrix.xenclient.db"
#define DBUS_VM_PATH     "/vm"
#define DBUS_BASE_PATH   "/"
#define DBUS_BUS_ADDR    "/var/run/dbus/system_bus_socket"
#define DBUS_INTRO_IFACE "org.freedesktop.DBus.Introspectable"
#define DBUS_INTRO_METH  "Introspect"


// src/json.c
struct json_response *init_jrsp(void);

struct json_response *make_json_request(struct json_request *jreq);

void load_json_response(DBusMessage *msg, struct json_response *jrsp);

struct json_request *convert_json_request(char *raw_json_req);

struct json_object *convert_dbus_response(struct json_response *jrsp);

void add_jobj(struct json_object *args, char *key, struct json_object *jobj);

void free_json_response(struct json_response *jrsp);

#define JSON_RESP "response"
#define JSON_SIG  "signal"
#define JSON_ID   "1"


// src/msg.c
int broker(struct dbus_message *dmsg, struct dbus_request *req);

int exchange(int rsock, int ssock, 
             ssize_t (*rcv)(int, void *, size_t, int),
             ssize_t (*snd)(int, const void *, size_t, int),
             struct dbus_request *req);

int filter(struct rule *policy_rule, struct dbus_message *dmsg, uint16_t domid);


// src/policy.c
struct policy *build_policy(const char *rule_filename);

void free_policy(void);


// src/signature.c
xmlNodePtr find_xml_property(const char *target, const xmlChar *property, 
                             xmlNodePtr node);

int retrieve_xml_signature(const xmlChar *xml_dump, char *args,
                           const char *interface, const char *member);

void parse_dbus_dict(struct json_object *args, char *key, DBusMessageIter *iter);

void parse_signature(struct json_object *args, char *key, DBusMessageIter *iter);


// src/websockets.c
char *prepare_json_reply(struct json_response *jrsp);

struct lws_context *create_ws_context(int port);

int ws_request_handler(struct lws *wsi, char *raw_req);

