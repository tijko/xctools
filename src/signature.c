#include "../rpc-broker.h"

xmlNodePtr find_xml_property(char *target, char *property, xmlNodePtr node)
{
    if (node == NULL)
        return NULL;

    char *name = xmlGetProp(node, property);

    if (name && !strcmp(name, target))
        return xmlFirstElementChild(node);

    node = find_xml_property(target, property, xmlNextElementSibling(node));

    return node;
}

int retrieve_xml_signature(char *xml_dump, char *args, 
                           char *iface, char *method)
{
    int idx = 0;
    char *error = NULL;

    xmlDocPtr doc = xmlParseDoc(xml_dump);
    xmlNodePtr root = xmlDocGetRootElement(doc);

    if (root == NULL) {
        error = "doc";
        goto xml_error;
    }

    xmlNodePtr iface_node = find_xml_property(iface, XML_NAME_PROPERTY, 
                                              xmlFirstElementChild(root));

    if (iface_node == NULL) {
        error = "interface";
        goto xml_error;
    }

    xmlNodePtr method_node = find_xml_property(method, XML_NAME_PROPERTY, 
                                               iface_node);

    if (method_node == NULL) {
        error = "method";
        goto xml_error;
    }

    char *name = xmlGetProp(method_node, XML_DIRECTION_PROPERTY);

    while (name && !strcmp(name, "in")) {

        char *type = xmlGetProp(method_node, "type");

        if (type)
            args[idx++] = type[0];

        method_node = xmlNextElementSibling(method_node);
        name = xmlGetProp(method_node, XML_DIRECTION_PROPERTY);
    }

xml_error:

    if (error)
        DBUS_BROKER_WARNING("Invalid xml-%s", error);

    args[idx] = '\0';

    return idx;
}

static inline void add_json_array(struct json_object *args, char *key, 
                                     DBusMessageIter *iter)
{
    DBusMessageIter sub;
    dbus_message_iter_recurse(iter, &sub);

    if (dbus_message_iter_get_arg_type(&sub) == DBUS_TYPE_INVALID && key) {
        struct json_object *empty_array = json_object_new_object();
        add_jobj(args, key, empty_array);
        return;
    }

    parse_signature(args, key, &sub);
}

void parse_dbus_dict(struct json_object *args, char *key, DBusMessageIter *iter)
{
    // json_object_put
    struct json_object *dbus_dict = json_object_new_object();
    add_jobj(args, key, dbus_dict);

    while (dbus_message_iter_get_arg_type(iter) != DBUS_TYPE_INVALID) {

        char *key = malloc(sizeof(char) * DBUS_ARG_LEN);

        DBusMessageIter sub;
        dbus_message_iter_recurse(iter, &sub);
        dbus_message_iter_get_basic(&sub, &key);

        dbus_message_iter_next(&sub);
        parse_signature(dbus_dict, key, &sub); 
        dbus_message_iter_next(iter);
    }
}

void parse_signature(struct json_object *args, char *key, DBusMessageIter *iter)
{
    int type;
    void *arg = malloc(sizeof(char) * DBUS_ARG_LEN);

    DBusMessageIter sub;
 
    while ((type = dbus_message_iter_get_arg_type(iter)) != DBUS_TYPE_INVALID) {

        struct json_object *obj = NULL;

        switch (type) {

            case (DBUS_TYPE_ARRAY): 
                add_json_array(args, key, iter);
                break;

            case (DBUS_TYPE_DICT_ENTRY):
                parse_dbus_dict(args, key, iter);
                break;

            case (DBUS_TYPE_OBJECT_PATH):
            case (DBUS_TYPE_STRING):
                dbus_message_iter_get_basic(iter, &arg);
                obj = json_object_new_string((char *) arg);
                break;

            case (DBUS_TYPE_INT32):
            case (DBUS_TYPE_UINT32):
                dbus_message_iter_get_basic(iter, arg);
                obj = json_object_new_int(*(int *) arg);
                break;

            case (DBUS_TYPE_DOUBLE):
                dbus_message_iter_get_basic(iter, arg);
                obj = json_object_new_double(*(double *) arg);
                break;

            case (DBUS_TYPE_BOOLEAN):
                dbus_message_iter_get_basic(iter, arg);
                obj = json_object_new_boolean(*(bool *) arg);
                break;

            case (DBUS_TYPE_VARIANT): {
                dbus_message_iter_recurse(iter, &sub);
                parse_signature(args, key, &sub);
                break;
            }

            case (DBUS_TYPE_INT64): {
                dbus_message_iter_get_basic(iter, arg);
                obj = json_object_new_int64(*(uint64_t *) arg);
                break;
            }

            default:
                DBUS_BROKER_WARNING("<dbus signature unrecognized> [Type: %d]",
                                      type);
                break;
        }

        if (obj != NULL)
            add_jobj(args, key, obj);

        dbus_message_iter_next(iter);
    }
}
