#include "../rpc-broker.h"


// create separate structures for each vm, where the /etc rules come first
// followed by its own domain-specific rules?
//
// Ex:
//      default = /etc
//      vm-1    = /etc + dom-1
//      vm-2    = /etc + dom-2
//
//

static inline void copy_rulelist(int count, struct rule **dest, 
                                            struct rule **src)
{
    for (int i=0; i < count; i++) {
        dest[i] = malloc(sizeof(struct rule));
        memcpy(dest[i], src[i], sizeof(struct rule));
    }
}

struct rule **build_domain_policy(int domid, struct policy *dbus_policy)
{
    struct rules *etc_rules = dbus_policy->etc_rules;
    int etc_count = etc_rules->count;
    struct rule **req_list = malloc(sizeof(struct rule *) * etc_count + 1);
    copy_rulelist(etc_count, req_list, etc_rules->rule_list);
    req_list[etc_count] = NULL;

    struct rules *dom_rules = dbus_policy->domain_rules;

    while (dom_rules && dom_rules->domid != domid)
        dom_rules = dom_rules->next;

    if (dom_rules) {
        int dom_count = dom_rules->count;
        req_list = realloc(req_list, sizeof(struct rule *) * 
                           (etc_count + dom_count + 1));
        copy_rulelist(dom_count, &(req_list[etc_count]), dom_rules->rule_list);
        req_list[etc_count + dom_count] = NULL;
    }

    return req_list;
}

int get_rules(DBusConnection *conn, struct rules *domain_rules)
{
    int rule_count = 0;
    char *rule = NULL;
    domain_rules->rule_list = NULL;

    do { 
        domain_rules->rule_list = realloc(domain_rules->rule_list, 
                                          sizeof(struct rule *) * 
                                                (rule_count + 1)); 

        domain_rules->rule_list[rule_count] = malloc(sizeof(struct rule));
        rule = db_rule_query(conn, domain_rules->uuid, rule_count);

        if (rule)
            domain_rules->rule_list[rule_count++]->rule_string = rule;
    } while (rule != NULL);

    domain_rules->count = rule_count;

    return 0;
}

void free_rule_list(struct rule **rule_list)
{
    struct rule **head = rule_list;

    while (*rule_list) {
        free((*rule_list)->rule_string);
        free(*rule_list);
        rule_list++;
    }

    free(head);
}

void free_rules(struct rules *policy_rules)
{
    if (!policy_rules)
        return;

    if (policy_rules->uuid)
        free(policy_rules->uuid);

    struct rules *current = policy_rules->next;

    while (current) {
        free_rule_list(current->rule_list);
        free_rules(current->next);
        free(current);
    }
    
    free(policy_rules);
}

void free_policy(struct policy *dbus_policy)
{
    free_rules(dbus_policy->domain_rules);
    free_rules(dbus_policy->etc_rules);
    free(dbus_policy);
}

struct policy *build_policy(const char *rule_filename)
{
    DBusConnection *conn = create_dbus_connection();

    if (!conn) 
        return NULL;

    struct dbus_message *dmsg = calloc(1, sizeof *dmsg);

    dbus_default(dmsg);
    dmsg->method = DBUS_LIST;
    dmsg->args[0] = (void *) DBUS_VM_PATH;

    DBusMessage *vms = make_dbus_call(conn, dmsg);
    if (dbus_message_get_type(vms) == DBUS_MESSAGE_TYPE_ERROR) {
        DBUS_BROKER_WARNING("<No policy in place> %s", "");
        return NULL;
    }

    DBusMessageIter iter, sub;
    dbus_message_iter_init(vms, &iter);
    dbus_message_iter_recurse(&iter, &sub); 

    struct policy *dbus_policy = malloc(sizeof *dbus_policy);
    dbus_policy->domain_rules = NULL;
    dbus_policy->etc_rules = NULL;
    struct rules *current = NULL;

    while (dbus_message_iter_get_arg_type(&sub) != DBUS_TYPE_INVALID) {

        void *arg = malloc(sizeof(char) * VM_UUID_LEN);
        dbus_message_iter_get_basic(&sub, &arg);

// add-link
        if (current == NULL) {
            current = malloc(sizeof *current);
            dbus_policy->domain_rules = current;
        } else {
            current->next = malloc(sizeof(struct rules));
            current = current->next;
        }

        current->next = NULL;
        current->uuid = arg;
        current->domid = strtol(arg + DOMID_SECTION, NULL, 10);
// re-use connection (return struct to next link-list)
        if (get_rules(conn, current))
            DBUS_BROKER_WARNING("<Error parsing rules> %s", "");

        dbus_message_iter_next(&sub);
    }

// split-off 
    dbus_policy->etc_rules = NULL;
    struct stat policy_stat;
    if (stat(rule_filename, &policy_stat) < 0)
        goto return_policy;

    size_t policy_size = policy_stat.st_size;
    char *policy = malloc(sizeof(char) * policy_size + 1);

    int policy_fd = open(rule_filename, O_RDONLY);

    if (policy_fd < 0)
        goto free_etc;

    int rbytes = read(policy_fd, policy, policy_size);
    close(policy_fd);

    policy[rbytes] = '\0';

    char *rule_token = strtok(policy, "\n");

    struct rules *etc_rules = malloc(sizeof(struct rules));
    etc_rules->rule_list = NULL;

    int idx = 0;

    while (rule_token != NULL) {

        etc_rules->rule_list = realloc(etc_rules->rule_list, 
                                       sizeof(struct rule *) * (idx + 1));
        etc_rules->rule_list[idx] = malloc(sizeof(struct rule));

        if (rule_token[0] != '#')
            etc_rules->rule_list[idx++]->rule_string = rule_token;

        rule_token = strtok(NULL, "\n");
    }

    etc_rules->count = idx;
    dbus_policy->etc_rules = etc_rules;

free_etc:
    free(policy);

return_policy:

    return dbus_policy;
}

