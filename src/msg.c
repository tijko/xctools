#include "../rpc-broker.h"


int broker(struct dbus_message *dmsg, struct dbus_request *req)
{
    // dbus-request only for domid and dom-rules
    int domid = req->domid;
    struct rule **rulelist = req->dom_rules;
    
    /*
     * Former-Method: here was to find an exact match which is technically 
     * incorrect.  The match could be over-ruled by a policy rule that is
     * listed in subsequent rules.
     */

    int policy = 0;

    for (int i=0; rulelist[i]; i++) {

        struct rule *current = rulelist[i];

        policy = filter(current, dmsg, domid);
    }

    char req_msg[1024];
    if (!dmsg->path)
        dmsg->path = "/";

    snprintf(req_msg, 1023, "Dom: %d [Dest: %s Path: %s Iface: %s Meth: %s]",
                      domid, dmsg->dest, dmsg->path, dmsg->iface, dmsg->method);
    
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

    if (((policy_rule->stubdom && stubdom_check(domid) < 1))             || 
        (policy_rule->dest && strcmp(policy_rule->dest, dmsg->dest))     ||
        (policy_rule->path && strcmp(policy_rule->path, dmsg->path))     ||
        (policy_rule->iface && strcmp(policy_rule->iface, dmsg->iface))  ||
        (policy_rule->member && strcmp(policy_rule->member, dmsg->method)))
        return 0;

    if (policy_rule->if_bool) {

        conn = create_dbus_connection();
        char *arg;
        char *base = DBUS_REQ_ARG(arg, "%d", domid);
        char *req = DBUS_REQ_ARG(arg, "/vm/00000000-0000-0000-00000000000%s/%s",
                                 base, policy_rule->if_bool);
        char *attr_cond = db_query(conn, req);
    
        if (!attr_cond || (attr_cond[0] == 't' && 
                           policy_rule->if_bool_flag == 0) ||
                          (attr_cond[0] == 'f' &&
                           policy_rule->if_bool_flag == 1))
            return 0;
    }

    if (policy_rule->domname) {

        conn = create_dbus_connection();
        char *arg;
        char *base = DBUS_REQ_ARG(arg, "%d", domid);
        char *uuid = DBUS_REQ_ARG(arg, "/vm/00000000-0000-0000-00000000000%s/type", base);
        char *dom_type = db_query(conn, arg);

        if (!dom_type || strcmp(policy_rule->domname, dom_type))
            return 0;
    }

    return policy_rule->policy;
}

