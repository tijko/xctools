#include "../rpc-broker.h"


int broker(struct dbus_message *dmsg, struct dbus_request *req)
{
    if (!dmsg || !req) {
        DBUS_BROKER_WARNING("Invalid broker request %s", "");
        return 1;
    }

    if (!req->domid) {
        DBUS_BROKER_WARNING("Invalid domain ID %s", "");
        return 1;
    }

    uint16_t domid = req->domid;

    int policy = 0;
    if (!dbus_broker_policy) {
        DBUS_BROKER_WARNING("No policy in place %s", "");
        return 1;
    }

    struct etc_policy etc = dbus_broker_policy->etc;
    int etc_count = etc.count;
    for (int i=0; i < etc_count; i++)
        policy = filter(&(etc.rules[i]), dmsg, domid);

    int domains = dbus_broker_policy->domain_number;
    
    for (int i=0; i < domains; i++) {
        if (dbus_broker_policy->domains[i].domid == domid) {

            struct domain_policy domain = dbus_broker_policy->domains[i];
            int domain_count = domain.count; 

            for (int j=0; j < domain_count; j++)
                policy = filter(&(domain.rules[j]), dmsg, domid);

            break;
        }
    }
        
    policy = 1;

    char req_msg[1024];

    snprintf(req_msg, 1023, "Dom: %d [Dest: %s Path: %s Iface: %s Meth: %s]",
                      domid, dmsg->destination, dmsg->path, 
                             dmsg->interface, dmsg->member);
    
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

            struct dbus_message dmsg;
            if (convert_raw_dbus(&dmsg, buf, len) < 0) {
                DBUS_BROKER_WARNING("DBus-message conversion failed %s", "");
                return 0;
            }

            if (broker(&dmsg, req) == 0) 
                return 0;   
        }     
    }

    if (rbytes < 1)
        return rbytes;

    snd(ssock, buf, rbytes, 0);

    return rbytes;
}

int filter(struct rule *policy_rule, struct dbus_message *dmsg, uint16_t domid)
{
    if (!policy_rule || !dmsg) {
        DBUS_BROKER_WARNING("Invalid filter request %s", "");
        return -1;
    }

    DBusConnection *conn;
    char *uuid, *arg;

    if (((policy_rule->stubdom && is_stubdom(domid) < 1))                || 
        (policy_rule->destination && strcmp(policy_rule->destination, 
                                                 dmsg->destination))     ||
        (policy_rule->path && strcmp(policy_rule->path, dmsg->path))     ||
        (policy_rule->interface && strcmp(policy_rule->interface, 
                                                 dmsg->interface))       ||
        (policy_rule->member && strcmp(policy_rule->member, dmsg->member)))
        return -1;

    if (policy_rule->if_bool || policy_rule->domtype) {
        conn = create_dbus_connection();
        struct xs_handle *xsh = xs_open(XS_OPEN_READONLY);

        size_t len;
        char path[256];
        snprintf(path, 255, "/local/domain/%d/vm", domid);
        uuid = (char *) xs_read(xsh, XBT_NULL, path, &len);
        xs_close(xsh);

        if (policy_rule->if_bool) {
            DBUS_REQ_ARG(arg, "%s/%s", uuid, policy_rule->if_bool);
            char *attr_cond = db_query(conn, arg);
        
            if (!attr_cond || (attr_cond[0] == 't' && 
                               policy_rule->if_bool_flag == 0) ||
                              (attr_cond[0] == 'f' &&
                               policy_rule->if_bool_flag == 1))
                return -1;

            free(arg);
        }

        if (policy_rule->domtype) {
            DBUS_REQ_ARG(arg, "%s/type", uuid);
            char *dom_type = db_query(conn, arg);
            if (!dom_type || strcmp(policy_rule->domtype, dom_type))
                return -1;
        }

        if (uuid)
            free(uuid);
    }

    return policy_rule->policy;
}

