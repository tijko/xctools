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

#include "rpc-broker.h"


/*
 * All requests are handled by this function whether they are raw requests
 * or Websocket requests.  For every policy rule listed either in the
 * /etc/rpc-broker.policy file or the domain-specific rules listed in the
 * xenclient database, each is passed to `filter` to determine whether or not
 * the message is dropped or passed through.
 */
int broker(struct dbus_message *dmsg, int domid)
{
    int policy, current_rule_policy, domains;
    struct etc_policy etc;

    int i, j;
    char req_msg[1024] = { '\0' };
    char *uuid;

    if (!dmsg) {
        DBUS_BROKER_WARNING("Invalid args to broker-request %s", "");
        return -1;
    }

    if (!dbus_broker_policy) {
        DBUS_BROKER_WARNING("No policy in place %s", "");
        return -1;
    }

    // deny by default
    policy = 0;
    // sets the current-rule-policy to default
    current_rule_policy = policy;

    etc = dbus_broker_policy->etc;

    for (i=0; i < etc.count; i++) {
        current_rule_policy = filter(&(etc.rules[i]), dmsg, domid);
        /*
         *  1 = the rule matched the request and rule's policy is allow
         *  0 = the rule matched the request and rule's policy is deny
         * -1 = the rule did *not* match the request
         *
         *  The filter is set up as last match, with the default being "deny".
         *  Iterating through all the rules, if a rule matches a request, then
         *  update the "policy" to fit with the "current-rule-policy"
         *  (whether thats allow or deny).  If there is no match (-1) then the
         *  "policy" remains unchanged, remaining set as it was before the
         *  rule being checked.
         */
        policy = current_rule_policy != -1 ? current_rule_policy : policy;
    }

    domains = dbus_broker_policy->domain_count;
    if (domain_uuids[domid])
        uuid = domain_uuids[domid];
    else {
        uuid = get_uuid_from_domid(domid);
        domain_uuids[domid] = uuid;
    }

    for (i=0; i < domains; i++) {
        if (uuid && !strcmp(dbus_broker_policy->domains[i].uuid_db_fmt, uuid)) {
            struct domain_policy domain = dbus_broker_policy->domains[i];

            for (j=0; j < domain.count; j++) {
                current_rule_policy = filter(&(domain.rules[j]), dmsg, domid);
                policy = current_rule_policy != -1 ? current_rule_policy : policy;
            }

            break;
        }
    }

    if (verbose_logging) {
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

void debug_raw_buffer(char *buf, int rbytes)
{
    char tmp[DBUS_MSG_LEN] = { '\0' };

    int i;
    for (i=0; i < rbytes; i++) {
        if (isalnum(buf[i]))
            tmp[i] = buf[i];
        else
            tmp[i] = '-';
    }

    DBUS_BROKER_EVENT("5555: %s", tmp);
}

/*
 * This is an opaque exchange reading off from the receiving end of a raw-dbus
 * connection.
 */
int exchange(int rsock, int ssock, uint16_t domid, bool is_client)
{
    struct dbus_message dmsg;
    int total, rbytes, len;
    char buf[DBUS_MSG_LEN] = { 0 };

    total = 0;
    rbytes = 0;

    while ((rbytes = recv(rsock, buf, DBUS_MSG_LEN, 0)) > 0) {
        if (rbytes > DBUS_COMM_MIN && is_client) {

            len = dbus_message_demarshal_bytes_needed(buf, rbytes);

            if (len == rbytes) {
                if (convert_raw_dbus(&dmsg, buf, len) < 1)
                    return -1;
                if (broker(&dmsg, domid) <= 0)
                    return -1;
            }
#ifdef DEBUG
        debug_raw_buffer(buf, rbytes);
#endif
        }

        total += rbytes;
        send(ssock, buf, rbytes, 0);
    }

    return total;
}

static inline char *get_db_vm_path(DBusConnection *conn, uint16_t domid)
{
#ifdef HAVE_XENSTORE
    size_t len;
    char path[256] = { 0 };
    char *uuid;
    struct xs_handle *xsh;

    xsh = xs_open(XS_OPEN_READONLY);

    if (!xsh)
       return NULL;

    snprintf(path, 255, "/local/domain/%d/vm", domid);

    uuid = (char *) xs_read(xsh, XBT_NULL, path, &len);
    xs_close(xsh);

    return uuid;
#endif
    return NULL;
}

static int filter_if_bool(DBusConnection *conn, char *uuid,
                          char *bool_cond, int bool_flag)
{
    char *arg, *attr_cond;
    int rc;

    rc = 0;
    arg = NULL;
    attr_cond = NULL;

    DBUS_REQ_ARG(arg, "%s/%s", uuid, bool_cond);
    attr_cond = db_query(conn, arg);
    free(arg);

    if (!attr_cond)
        return -1;

    if ((!strcmp("true", attr_cond) && bool_flag == 0) ||
        (!strcmp("false", attr_cond) && bool_flag == 1))
        rc = -1;

    free(attr_cond);

    return rc;
}

static int filter_domtype(DBusConnection *conn, char *uuid,
                                                char *policy_domtype)
{
    int ret;
    char *arg, *dom_type;

    DBUS_REQ_ARG(arg, "%s/type", uuid);
    dom_type = db_query(conn, arg);
    free(arg);

    ret = 0;
    if (dom_type && strcmp(policy_domtype, dom_type))
        ret = -1;

    if (dom_type)
        free(dom_type);

    return ret;
}

/*
 * The main policy filtering function, compares the policy-rule against the dbus
 * request being made.
 */
int filter(struct rule *policy_rule, struct dbus_message *dmsg, uint16_t domid)
{
    DBusConnection *conn;
    char *uuid;
    int filter_policy;

    if (!policy_rule || !dmsg) {
        DBUS_BROKER_WARNING("Invalid filter request %s", "");
        return -1;
    }

    filter_policy = policy_rule->policy;
    conn = NULL;
    uuid = NULL;

    if ((policy_rule->stubdom && is_stubdom(domid) < 1)                  ||
        (policy_rule->destination && strcmp(policy_rule->destination,
                                                 dmsg->destination))     ||
        (policy_rule->path && strcmp(policy_rule->path, dmsg->path))     ||
        (policy_rule->interface && strcmp(policy_rule->interface,
                                                 dmsg->interface))       ||
        (policy_rule->member && strcmp(policy_rule->member, dmsg->member))) {
        filter_policy = -1;
        goto policy_set;
    }

    if (policy_rule->if_bool || policy_rule->domtype) {
        conn = create_dbus_connection();
        uuid = get_db_vm_path(conn, domid);
        if (uuid == NULL) {
            filter_policy = -1;
            goto policy_set;
        }

        if (policy_rule->if_bool &&
            filter_if_bool(conn, uuid, (char *) policy_rule->if_bool,
                                       policy_rule->if_bool_flag) < 0) {
            filter_policy = -1;
            goto policy_set;
        }

        if (policy_rule->domtype &&
            filter_domtype(conn, uuid, (char *) policy_rule->domtype) < 0) {
            filter_policy = -1;
            goto policy_set;
        }
    }

policy_set:

    if (uuid)
        free(uuid);

    return filter_policy;
}

