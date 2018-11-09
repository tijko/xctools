/*
 Copyright (c) 2018 AIS, Inc.

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include "../rpc-broker.h"


/*
 * All requests are handled by this function whether they are raw requests 
 * coming from port 5555 or Websocket requests from 8080.  For every policy
 * rule listed either in /etc/rpc-broker.policy file or the domain-specific
 * rules listed in xenclient database are passed to `filter` to determine
 * whether or not to be dropped or passed through.
 */
int broker(struct dbus_message *dmsg, int domid)
{
    if (!dmsg) {
        DBUS_BROKER_WARNING("Invalid args to broker-request %s", "");
        return 1;
    }

    int policy = 0;

    if (!dbus_broker_policy) {
        DBUS_BROKER_WARNING("No policy in place %s", "");
        return 1;
    }

    struct etc_policy etc = dbus_broker_policy->etc;

    int i, j;
    for (i=0; i < etc.count; i++)
        policy = filter(&(etc.rules[i]), dmsg, domid);

    int domains = dbus_broker_policy->domain_number;

    for (i=0; i < domains; i++) {
        if (dbus_broker_policy->domains[i].domid == domid) {

            struct domain_policy domain = dbus_broker_policy->domains[i];

            for (j=0; j < domain.count; j++)
                policy = filter(&(domain.rules[j]), dmsg, domid);

            break;
        }
    }

    if (verbose_logging) {
        char req_msg[1024];

        snprintf(req_msg, 1023, "Dom: %d [Dest: %s Path: %s Iface: %s Meth: %s]",
                          domid, dmsg->destination, dmsg->path,
                                 dmsg->interface, dmsg->member);

        if (policy == 0)
            DBUS_BROKER_WARNING("%s <%s>", req_msg, "Dropped request");
        else
            DBUS_BROKER_EVENT("%s <%s>", req_msg, "Passed request");
    }

    return policy;
}

/*
 * This is an opaque function to pass raw dbus-bytes back in forth between
 * sender and receiver.  Its opaque in the sense that the function is unaware
 * who the client is and who the server is.  There are just sender and receiver
 * syscalls being made over port 5555.
 */
int exchange(int rsock, int ssock,
             ssize_t (*rcv)(int, void *, size_t, int),
             ssize_t (*snd)(int, const void *, size_t, int),
             int domid)
{
    int total = 0;
    char buf[DBUS_MSG_LEN] = { 0 };
    fd_set recv_fd;

    while (1) {

        struct timeval tv = { .tv_sec=0, .tv_usec= DBUS_BROKER_MSG_TIMEOUT };
        FD_ZERO(&recv_fd);
        FD_SET(rsock, &recv_fd);

        if (select(rsock + 1, &recv_fd, NULL, NULL, &tv) <= 0)
            break;

        int rbytes = rcv(rsock, buf, DBUS_MSG_LEN, 0);

        if (rbytes <= 0)
            break;

        if (rbytes > DBUS_COMM_MIN) { 

            struct dbus_message dmsg;
            int len = dbus_message_demarshal_bytes_needed(buf, rbytes);

            if (len == rbytes) {

                if (convert_raw_dbus(&dmsg, buf, len) < 1)
                    return -1;

                if (!strcmp(dmsg.member, "AddMatch")) 
                    add_raw_signal(rsock, ssock);

            //    if (broker(&dmsg, domid) < 1)
            //        return -1;
            }
        }

        //
        char log[DBUS_MSG_LEN] = { 0 };
        printf("Msg <%d>: ", rbytes);
        for (int i=0; i < rbytes; i++) {
            if (buf[i] == '\0' ||
                buf[i] == '\n' ||
                buf[i] == '\r') {
                printf("-");
                log[i] = '-';
            } else {
                printf("%c", buf[i]);
                log[i] = buf[i];
            }
        }
        log[rbytes] = '\0';
        DBUS_BROKER_EVENT("5555: %s\n", log); 
        printf("\n");
        //

        total += rbytes; 
        int sbytes = snd(ssock, buf, rbytes, 0);
        //
        printf("Sent: %d\n", sbytes);
    }

    return total;            
}

static inline char *get_uuid(DBusConnection *conn, uint16_t domid)
{
    size_t len;
    char path[256] = { 0 };

    struct xs_handle *xsh = xs_open(XS_OPEN_READONLY);

    if (!xsh)
       return NULL;

    snprintf(path, 255, "/local/domain/%d/vm", domid);

    char *uuid = (char *) xs_read(xsh, XBT_NULL, path, &len);
    xs_close(xsh);

    return uuid;
}

static int filter_if_bool(DBusConnection *conn, char *uuid, 
                          char *bool_cond, int bool_flag)
{
    char *arg = NULL;
    char *attr_cond = NULL;

    DBUS_REQ_ARG(arg, "%s/%s", uuid, bool_cond);
    attr_cond = db_query(conn, arg);
    free(arg); 

    if (!attr_cond || (attr_cond[0] == 't' && bool_flag == 0) || 
                      (attr_cond[0] == 'f' && bool_flag == 1)) {  
        free(uuid);
        return -1;
    }

    if (attr_cond)
        free(attr_cond);

    return 0;
}

static int filter_domtype(DBusConnection *conn, char *uuid, 
                                                char *policy_domtype)
{
    char *arg;

    DBUS_REQ_ARG(arg, "%s/type", uuid);
    char *dom_type = db_query(conn, arg);
    free(arg);

    if (dom_type && strcmp(policy_domtype, dom_type)) {
        free(dom_type); 
        free(uuid); 
        return -1;
    }

    if (dom_type)
        free(dom_type);

    return 0;
}

/*
 * The main policy filtering function, compares the policy-rule against the dbus
 * request being made.
 */
int filter(struct rule *policy_rule, struct dbus_message *dmsg, uint16_t domid)
{
    if (!policy_rule || !dmsg) {
        DBUS_BROKER_WARNING("Invalid filter request %s", "");
        return -1;
    }

    DBusConnection *conn = NULL;
    char *uuid = NULL;

    if ((policy_rule->stubdom && is_stubdom(domid) < 1)                  ||
        (policy_rule->destination && strcmp(policy_rule->destination,
                                                 dmsg->destination))     ||
        (policy_rule->path && strcmp(policy_rule->path, dmsg->path))     ||
        (policy_rule->interface && strcmp(policy_rule->interface,
                                                 dmsg->interface))       ||
        (policy_rule->member && strcmp(policy_rule->member, dmsg->member)))
        return -1;

    if (policy_rule->if_bool || policy_rule->domtype) {
        conn = create_dbus_connection();
        if (!(uuid = get_uuid(conn, domid)));
            return -1;

        if (policy_rule->if_bool && 
            filter_if_bool(conn, uuid, policy_rule->if_bool, 
                                       policy_rule->if_bool_flag) < 0) {
            return -1;
        }

        if (policy_rule->domtype && 
            filter_domtype(conn, uuid, policy_rule->domtype) < 0) {
            return -1;
        }
    }

    if (uuid)
        free(uuid);

    return policy_rule->policy;
}

