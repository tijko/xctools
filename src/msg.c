#include "../rpc-broker.h"


int broker(struct dbus_message *dmsg, struct dbus_request *req)
{
    char *dest = dmsg->dest;
    char *iface = dmsg->iface;
    char *member = dmsg->method;
    int domid = req->domid;
    struct rule **rulelist = req->dom_rules;

    for (int i=0; rulelist[i]; i++) {
        char *rule = rulelist[i]->rule_string;
        // check rule?
        if (!strstr(rule, dest))
            continue;

        if (filter(dest, iface, member, rule)) {
            DBUS_BROKER_WARNING("%s Dom: %d [Dest: %s Iface: %s Member: %s]",
                               "<Dropped request>", domid, dest, iface, member);
            return 1;
        } 
    }

    DBUS_BROKER_EVENT("%s Dom: %d [Dest: %s Iface: %s Member: %s]",
                      "<Pass request>", domid, dest, iface, member);

    return 0;
}

int exchange(int rsock, int ssock, ssize_t (*rcv)(int, void *, size_t, int),
                                   ssize_t (*snd)(int, void *, size_t, int),
             struct dbus_request *req)
{
    char buf[DBUS_MSG_LEN];

    int rbytes = rcv(rsock, buf, DBUS_MSG_LEN, 0);

    // why 8-bytes?
    if (rbytes >= 8) {

        int len = dbus_message_demarshal_bytes_needed(buf, rbytes);

        if (len == rbytes) {
            struct dbus_message *dmsg = convert_raw_dbus(buf, len);
            if (broker(dmsg, req))
                return 0;   
        }     
    }

    if (rbytes < 1)
        return rbytes;

    snd(ssock, buf, rbytes, 0);

    return rbytes;
}

int filter(char *dest, char *iface, char *member, char *rule)
{
    char buf[RULE_MAX_LENGTH];
    char *request = malloc(sizeof(char) * RULE_MAX_LENGTH);

    if (rule[0] == 'a') 
        strcpy(request, "allow");
    else
        strcpy(request, "deny");

    char *req_args[] = { dest, iface, member };
    // the optional start of "dom-type" before destination
    // the optional end of "if-boolean" after member
    char *directives[] = { "destination", "interface", "member" };
    int policy = 0;
    
    for (int i=0; i < 3; i++) {
        snprintf(buf, RULE_MAX_LENGTH - 1, " %s %s", directives[i], 
                                                      req_args[i]);
        strcat(request, buf);
        if (!strcmp(rule, request)) {
            policy = rule[0] == 'a' ? 0 : 1;
            break;
        }
    }

    free(request);
    return policy;
}

