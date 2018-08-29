#include "../rpc-broker.h"


int broker(struct dbus_message *dmsg, struct dbus_request *req)
{
    int domid = req->domid;
    
    struct rule **rulelist = req->dom_rules;
    int policy = 0;

    for (int i=0; rulelist[i]; i++) {
        struct rule *current = rulelist[i];
        policy = filter(current, dmsg, domid);
        policy = 1;
    }

    char req_msg[1024];
    if (!dmsg->path)
        dmsg->path = "/";

    snprintf(req_msg, 1023, "Dom: %d [Dest: %s Path: %s Iface: %s Meth: %s]",
                      domid, dmsg->dest, dmsg->path, dmsg->interface, dmsg->member);
    
    if (policy == 0)
        DBUS_BROKER_WARNING("%s <%s>", req_msg, "Dropped request");
    else
        DBUS_BROKER_EVENT("%s <%s>", req_msg, "Passed request");

    return policy;
}

int exchange(int rsock, int ssock, 
             ssize_t (*rcv)(int, void *, size_t, int),
             ssize_t (*snd)(int, const void *, size_t, int),
             struct dbus_request *req)
{
    char buf[DBUS_MSG_LEN];
    int rbytes = rcv(rsock, buf, DBUS_MSG_LEN, 0);

    // why 8-bytes?
    if (rbytes >= 8) {

        int len = dbus_message_demarshal_bytes_needed(buf, rbytes);

        if (len == rbytes) {
            struct dbus_message *dmsg = convert_raw_dbus(buf, len);
            if (broker(dmsg, req) == 0) 
                return 0;   
        }     
    }

    if (rbytes < 1)
        return rbytes;

    snd(ssock, buf, rbytes, 0);

    return rbytes;
}

int filter(struct rule *policy_rule, struct dbus_message *dmsg, int domid)
{
    DBusConnection *conn;
    char *uuid, *arg, *query;

    if (((policy_rule->stubdom && is_stubdom(domid) < 1))                || 
        (policy_rule->dest && strcmp(policy_rule->dest, dmsg->dest))     ||
        (policy_rule->path && strcmp(policy_rule->path, dmsg->path))     ||
        (policy_rule->interface && strcmp(policy_rule->interface, 
                                                 dmsg->interface))       ||
        (policy_rule->member && strcmp(policy_rule->member, dmsg->member)))
        return 0;

    if (policy_rule->if_bool || policy_rule->domname) {
        conn = create_dbus_connection();
        struct xs_handle *xsh = xs_open(XS_OPEN_READONLY);

        size_t len;
        snprintf(path, 255, "/local/domain/%d/vm", domid);
        char path[256];
        uuid = (char *) xs_read(xsh, XBT_NULL, path, &len);
        xs_close(xsh);

        if (policy_rule->if_bool) {
            DBUS_REQ_ARG(arg, "%s/%s", uuid, policy_rule->if_bool);
            char *attr_cond = db_query(conn, arg);
        
            if (!attr_cond || (attr_cond[0] == 't' && 
                               policy_rule->if_bool_flag == 0) ||
                              (attr_cond[0] == 'f' &&
                               policy_rule->if_bool_flag == 1))
                return 0;

            free(arg);
        }

        if (policy_rule->domname) {
            DBUS_REQ_ARG(arg, "%s/type", uuid);
            char *dom_type = db_query(conn, arg);
            if (!dom_type || strcmp(policy_rule->domname, dom_type))
                return 0;
        }

        if (uuid)
            free(uuid);
    }

    return policy_rule->policy;
}

