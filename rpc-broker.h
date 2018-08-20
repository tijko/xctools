#define _GNU_SOURCE
#include <dbus/dbus.h>
#include <json.h>
#include <libwebsockets.h>
#include <libv4v.h>
#include <libxml/parser.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <sys/socket.h>
#include <sys/un.h>
//
#include <xenstore.h>
//
struct lws_ring *ring;
sem_t *memory_lock;
//

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

#define BROKER_DEFAULT_PORT 5555
#define BROKER_UI_PORT      8080
        
#define RULES_FILENAME "/etc/rpc-proxy.rules"
#define RULES_MAX_LENGTH 256
#define RULE_MAX_LENGTH  256

#define DBUS_REQ_TIMEOUT 1000
#define DBUS_MSG_LEN     8192
#define DBUS_ARG_LEN     1024

#define DBUS_READ "read"
#define DBUS_LIST "list"

#define CLIENT_REQ_LEN       16
#define CLIENT_DBUS_REQ_LEN 256

#define DBUS_DB_DEST     "com.citrix.xenclient.db"
#define DBUS_DB_IFACE    "com.citrix.xenclient.db"
#define DBUS_VM_PATH     "/vm"
#define DBUS_BASE_PATH   "/"
#define DBUS_BUS_ADDR    "/var/run/dbus/system_bus_socket"
#define DBUS_INTRO_IFACE "org.freedesktop.DBus.Introspectable"
#define DBUS_INTRO_METH  "Introspect"

#define DBUS_BUS_ADDR_LEN 256
#define DBUS_MAX_ARG_LEN   16

#define VM_UUID_LEN   33
#define DOMID_UUID_LEN 8
#define DOMID_SECTION 24

#define WS_RING_BUFFER_MEMBER_SIZE 4096
#define WS_RING_BUFFER_MEMBER_NUM    64

#define WS_USER_MEM_SIZE 8192  // the amount memory that is allocated for user
                               // for each ws-callback
                               //
#define WS_LOOP_TIMEOUT  2000  // length of time each service of the websocket 
                               // event-loop (millisecs)

#define DBUS_BROKER_TIMEOUT 100

#define XML_NAME_PROPERTY      "name"
#define XML_DIRECTION_PROPERTY "direction"

#define JSON_REQ_ID_MAX 16
#define JSON_REQ_MAX   256
#define JSON_RESP_TYPE "response"
#define JSON_RESP_ID   "1"

struct dbus_broker_server {
    int dbus_socket;
	v4v_addr_t addr;
	v4v_addr_t peer;
};

struct dbus_request {
    int domid;
    int client;
    struct rule **dom_rules;
};

// linked-list of per-domain rule(s)
struct rules {
    int domid;
    int count;
    char *uuid;
    struct rules *next;
    struct rule **rule_list;
};

struct rule {
    int policy:1; 
    int if_bool_flag:1; 
    char *dest;
    char *path;
    char *iface;
    char *member;
    char *if_bool;
    char *rule_string;
};

struct dbus_message {
    const char *dest;
    const char *iface;
    const char *path;
    const char *method;
    // struct argument {
    //   void *data;
    //   char dbus_type;
    //   char actual_type;
    // };
    int arg_number;
    char arg_sig[DBUS_MAX_ARG_LEN];
    char json_sig[DBUS_MAX_ARG_LEN];
    void *args[DBUS_MAX_ARG_LEN];
};

struct json_request {
    int id;
    DBusConnection *conn;
    struct lws *wsi;
    struct dbus_message *dmsg;
};

struct json_response {
    int id;
    char *response_to;
    char *type;        // set as bitfield?
    const char *path;
    const char *iface;
    const char *meth;
    char *arg_sig;
    struct json_object *args;
};

struct json_request *convert_json_request(char *raw_json_req);
struct json_response *make_json_request(struct json_request *jreq);
struct json_object *convert_dbus_response(struct json_response *jrsp);
struct dbus_message *convert_raw_dbus(const char *msg, size_t len);

int parse_json_args(struct json_object *jarray, struct json_request *jreq);
void add_jobj(struct json_object *args, char *key, struct json_object *jobj);
void parse_dbus_dict(struct json_object *args, char *key, DBusMessageIter *iter);

struct broker_signal {
    DBusConnection *conn;
    struct lws *wsi;
};

struct policy {
    int vm_number;
    struct rules *etc_rules;
    struct rules *domain_rules;
};

struct dbus_broker_args {
    bool logging;
    bool verbose;
    char *bus_name;
	char *logging_file;
	char *rule_file;
};

struct policy *dbus_broker_policy;

void free_rule_list(struct rule **rule_list);
void free_rules(struct rules *policy_rules);
void free_policy(struct policy *dbus_policy);
 
struct policy *build_policy(const char *rule_filename);

int broker(struct dbus_message *dmsg, struct dbus_request *req);

void *broker_message(void *request);

int connect_to_system_bus(void);

char *db_rule_query(DBusConnection *conn, char *uuid, int rule_number);

void *dbus_request(void *_req);

int exchange(int rsock, int ssock, 
             ssize_t (*rcv)(int, void *, size_t, int),
             ssize_t (*snd)(int, const void *, size_t, int),
             struct dbus_request *req);

int filter(const char *dest, const char *iface, 
           const char *member, char *rule);

// split-up according to functional relation
int get_rules(DBusConnection *conn, struct rules *policy_rules);

int init_request(int client, struct policy *dbus_policy);

void print_usage(void);

void sigint_handler(int signal);

int ws_request_handler(struct lws *wsi, char *raw_req);

void run(struct dbus_broker_args *args);

DBusMessage *make_dbus_call(DBusConnection *conn, struct dbus_message *dmsg);

DBusConnection *create_dbus_connection(void);

void dbus_default(struct dbus_message *dmsg);

struct dbus_broker_server *start_server(int port);

char *dbus_introspect(struct json_request *jreq);

xmlNodePtr find_xml_property(char *target, char *property, xmlNodePtr node);

int retrieve_xml_signature(char *xml_dump, char *args,
                           char *iface, char *method);

void parse_signature(struct json_object *args, char *key, DBusMessageIter *iter);

struct rule **build_domain_policy(int domid, struct policy *dbus_policy);

struct lws_context *create_ws_context(int port);

void load_json_response(DBusMessage *msg, struct json_response *jrsp);

char *prepare_json_reply(struct json_response *jrsp);

struct json_response *init_jrsp(void);
