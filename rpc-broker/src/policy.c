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


static int create_rule(struct rule *current, char *rule)
{
    char *token;
    const char *delimiter;

    if (!rule || rule[0] == '\0')
        return -1;

    current->rule_string = strdup(rule);

    delimiter = " ";
    token = strtok(rule, delimiter);

    if (!token)
        return -1;

    /* The first field predicates the policy "allow" or "deny" */
    current->policy = strcmp(token, "allow") == 0;

    token = strtok(NULL, delimiter);
    if (!strcmp(token, "all")) {
        current->all = 1;
        return 0;
    }

    while (token) {

        char *field = strtok(NULL, delimiter);

        if (!field && token[0] != 's') {
            return -1;
        } else if (!strcmp("destination", token)) {
            current->destination = strdup(field);
        } else if (!strcmp("dom-type", token)) {
            current->domtype = strdup(field);
        } else if (!strcmp("interface", token)) {
            current->interface = strdup(field);
        } else if (!strcmp("if-boolean", token)) {
            current->if_bool = strdup(field);
            token = strtok(NULL, delimiter);
            current->if_bool_flag = strcmp("true", token) == 0 ? 1 : 0;
        } else if (!strcmp("stubdom", token)) {
            current->stubdom = 1;
        } else if (!strcmp("path", token)) {
            current->path = strdup(field);
        } else if (!strcmp("member", token)) {
            current->member = strdup(field);
        } else if (!strcmp("out-any", token)) {
            current->out = 1;
        } else {
            DBUS_BROKER_WARNING("Unrecognized Rule-Token: %s", token);
            free_rule(*current);
            current = NULL;
            return -1;
        }

        token = strtok(NULL, delimiter);
    }

    return 0;
}

static inline void get_rules(DBusConnection *conn, struct domain_policy *dom)
{
    int rule_idx;
    char *rulestring;
    char *arg;
    struct rule *policy_rule;

    for (rule_idx=0; rule_idx < MAX_RULES; rule_idx++) {
        DBUS_REQ_ARG(arg, "/vm/%s/rpc-firewall-rules/%d",
                     dom->uuid, rule_idx);

        rulestring = db_query(conn, arg);
        free(arg);

        if (!rulestring)
            break;

        policy_rule = &(dom->rules[rule_idx]);

        if (create_rule(policy_rule, rulestring) < 0)
            free_rule(*policy_rule);
        else
            dom->count++;

        free(rulestring);
    }

}

static void build_etc_policy(struct etc_policy *etc, const char *rule_filepath)
{
    int rule_idx;
    FILE *policy_fh;
    size_t rbytes, line_length;
    char *line;
    char current_rule[RULE_MAX_LENGTH] = { 0 };
    struct rule *current;

    rule_idx = 0;
    etc->count = 0;

    policy_fh = fopen(rule_filepath, "r");
    if (!policy_fh) {
        DBUS_BROKER_WARNING("/etc policy stat of file <%s> failed %s",
                             rule_filepath, strerror(errno));
        return;
    }

    line = NULL;

    while ((getline(&line, &rbytes, policy_fh) > 0) && rule_idx < MAX_RULES) {

        if (rbytes > RULE_MAX_LENGTH - 1) {
            DBUS_BROKER_WARNING("Invalid policy rule %zu exceeds max-rule",
                                                                  rbytes);
        } else if (line && isalpha(line[0])) {
            line_length = strlen(line);
            line[line_length - 1] = '\0';
            memcpy(current_rule, line, rbytes);
            current = &(etc->rules[rule_idx]);
            create_rule(current, current_rule) < 0 ? free_rule(*current) :
                                                               rule_idx++;
        }

        if (line)
            free(line);

        line = NULL;
    }

    if (line)
        free(line);

    etc->count = rule_idx;
    fclose(policy_fh);
}

struct policy *build_policy(const char *rule_filename)
{
    struct policy *dbus_policy;
    struct etc_policy *etc;
    int dom_idx;
    DBusMessage *vms;
    DBusConnection *conn;
    DBusMessageIter iter, sub;
    void *arg;
    struct domain_policy *current;
    char uuid[64];

    dbus_policy = calloc(1, sizeof *dbus_policy);
    etc = &(dbus_policy->etc);
    build_etc_policy(etc, rule_filename);
    dbus_policy->domain_count= 0;

    dom_idx = 0;
    vms = db_list();
    if (!vms)
        return dbus_policy;
    conn = create_dbus_connection();

    dbus_message_iter_init(vms, &iter);
    dbus_message_iter_recurse(&iter, &sub);

    while (dbus_message_iter_get_arg_type(&sub) != DBUS_TYPE_INVALID) {

        dbus_message_iter_get_basic(&sub, &arg);
        current = &(dbus_policy->domains[dom_idx]);
        strcpy(current->uuid, arg);
        // alter the uuid from underscores to dashes
        // dbus returns underscores while the vm db uses dashes
        // if not the vm db policy check on strcmp on the uuid is off
        TRANSFORM_UUID(arg, uuid);
        strcpy(current->uuid_db_fmt, uuid);

        errno = 0;
        if (errno != 0) {
            DBUS_BROKER_WARNING("Domain ID error <%s>", strerror(errno));
            continue;
        }

        get_rules(conn, current);

        dbus_message_iter_next(&sub);
        dom_idx++;
    }

    dbus_policy->domain_count = dom_idx;
    dbus_message_unref(vms);
    dbus_connection_unref(conn);

    return dbus_policy;
}

void free_rule(struct rule r)
{
    if (r.destination)
        free((char *) (r.destination));

    if (r.path)
        free((char *) (r.path));

    if (r.interface)
        free((char *) (r.interface));

    if (r.member)
        free((char *) (r.member));

    if (r.if_bool)
        free((char *) (r.if_bool));

    if (r.domtype)
        free((char *) (r.domtype));

    if (r.rule_string)
        free((char *) (r.rule_string));
}

void free_policy(void)
{
    int count;
    struct domain_policy domain;
    struct etc_policy etc;
    int i, j;

    count = dbus_broker_policy->domain_count;
    for (i=0; i < count; i++) {

        domain = dbus_broker_policy->domains[i];
        for (j=0; j < domain.count; j++)
            free_rule(domain.rules[j]);
    }

    etc = dbus_broker_policy->etc;

    for (i=0; i < etc.count; i++)
        free_rule(etc.rules[i]);

    free(dbus_broker_policy);
}

