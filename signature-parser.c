#include <dbus/dbus.h>
#include <json.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define ERROR(call)                                                 \
    do {                                                            \
        printf("Error in Function(%s) Call(%s)\n", __func__, call); \
        exit(0);                                                    \
    } while ( 1 )                                                   \

void parse_signature(struct json_object *args, char *key, DBusMessageIter *iter);
void new_array(struct json_object *args, char *key, DBusMessageIter *iter);

void add_jobj(struct json_object *args, char *key, struct json_object *jobj)
{
    if (!key)
        json_object_array_add(args, jobj);
    else
        json_object_object_add(args, key, jobj);
}

void parse_dict(struct json_object *args, char *key, DBusMessageIter *iter)
{
    struct json_object *dbus_dict = json_object_new_object();
    add_jobj(args, key, dbus_dict);

    while (dbus_message_iter_get_arg_type(iter) != DBUS_TYPE_INVALID) {

        // XXX set a flag for rare case of differently typed (special) keys
        char *key = malloc(sizeof(char) * 1024);

        DBusMessageIter sub;
        dbus_message_iter_recurse(iter, &sub);
        dbus_message_iter_get_basic(&sub, &key);

        dbus_message_iter_next(&sub);
        parse_signature(dbus_dict, key, &sub); 
        dbus_message_iter_next(iter);
    }
}

void new_array(struct json_object *args, char *key, DBusMessageIter *iter)
{
    struct json_object *array = json_object_new_array();

    add_jobj(args, key, array);

    DBusMessageIter sub;
    dbus_message_iter_recurse(iter, &sub);

    parse_signature(array, NULL, &sub);
}

void parse_signature(struct json_object *args, char *key, DBusMessageIter *iter)
{
    int type;
    void *arg = malloc(sizeof(char) * 1024);
    DBusMessageIter sub;

    while ((type = dbus_message_iter_get_arg_type(iter)) != DBUS_TYPE_INVALID) {

        struct json_object *obj = NULL;

        switch (type) {

            case (DBUS_TYPE_ARRAY): 
                new_array(args, key, iter);
                break;

            case (DBUS_TYPE_DICT_ENTRY):
                parse_dict(args, key, iter);
                break;

            case (DBUS_TYPE_STRING):
                dbus_message_iter_get_basic(iter, (char **) &arg);
                obj = json_object_new_string((char *) arg);
                break;

            case (DBUS_TYPE_INT32):
            case (DBUS_TYPE_UINT32):
                dbus_message_iter_get_basic(iter, (int *) &arg);
                obj = json_object_new_int(*(int *) arg);
                break;

            case (DBUS_TYPE_DOUBLE):
                dbus_message_iter_get_basic(iter, (double *) &arg);
                obj = json_object_new_double(*(double *) arg);
                break;

            case (DBUS_TYPE_BOOLEAN):
                dbus_message_iter_get_basic(iter, (bool *) &arg);
                obj = json_object_new_boolean(*(bool *) arg);
                break;

            case (DBUS_TYPE_VARIANT): {
                dbus_message_iter_recurse(iter, &sub);
                parse_signature(args, key, &sub);
                break;
            }

            default:
                break;
        }

        if (obj != NULL)
            add_jobj(args, key, obj);

        dbus_message_iter_next(iter);
    }
}

int main(int argc, char *argv[])
{
    printf("Connecting to system bus...\n");

    struct json_object *args = json_object_new_array();
    DBusConnection *conn = dbus_bus_get(DBUS_BUS_SYSTEM, NULL);

    if (!conn) 
        ERROR("dbus_bus_get");

    DBusMessage *msg = dbus_message_new_method_call("org.freedesktop.DBus", "/org/freedesktop/DBus",
                                                    "org.freedesktop.DBus.Debug.Stats", "GetAllMatchRules");

    if (!msg)
        ERROR("dbus_message_method_call_new");

    dbus_connection_flush(conn);

    DBusPendingCall *pc;
    dbus_connection_send_with_reply(conn, msg, &pc, 100000);


    dbus_pending_call_block(pc);
    msg = dbus_pending_call_steal_reply(pc);
    if (!msg)
        ERROR("dbus_pending_call_steal_reply");

    DBusMessageIter iter;
    dbus_message_iter_init(msg, &iter);

    printf("Message signature: %s\n", dbus_message_iter_get_signature(&iter));
    
    parse_signature(args, NULL, &iter);
    printf("%s\n", json_object_to_json_string(args));

    return 0;
}

