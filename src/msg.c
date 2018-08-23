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
    snprintf(req_msg, 1023, "%s Dom: %d [Dest: %s Path: %s Iface: %s Meth: %s]",
                      domid, dmsg->dest, dmsg->path, dmsg->iface, dmsg->member);
    
    if (policy == 0)
        DBUS_BROKER_WARNING("%s <%s>", req_msg, "Dropped request");
    else
        DBUS_BROKER_EVENT("%s <%s>", req_msg "Passed request");

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
    // on dom-type make-dbus-call with uuid vm defined? xenclient-db
    // on if-boolean make-dbus-call (gets all attributes?) xenclient-db
    // if dmsg makes it thru return policy_rule
    if (policy_rule->stubdom && stubdom_check(domid) < 1) 
        return 0;    

    if (policy_rule->dest && strcmp(policy_rule->dest, dmsg->dest))
        return 0;

    if (policy_rule->path && strcmp(policy_rule->path, dmsg->path))
        return 0;

    if (policy_rule->iface && strcmp(policy_rule->iface, dmsg->iface))
        return 0;

    if (policy_rule->member && strcmp(policy_rule->member, dmsg->method))
        return 0;

    if (policy_rule->if_bool) {
        // look-up "if-boolean token (set field for condition)
        // xenclient-db attribute look-up (dbus-make-call --> db)
        // if conditions doesn't exist
        // or condition value don't match return 0
    }

    if (policy_rule->domname) {
        // make xenclient-db lookup name of domid
        // make-dbus-call
        // return 0 if don't match
    }

    return policy_rule->policy;
}

