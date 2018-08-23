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

struct rule *create_rule(char *rule)
{
    // How-to interact with "out-any" 
    // (i think Jed mentioned to ignore it for now)
    struct rule *current = calloc(1, sizeof(struct rule));
    current->rule_string = strdup(rule);

    char *ruleptr;
    char *delimiter = " ";
    char *token = strtok_r(rule, delimiter, &ruleptr);

    current->policy = token[0] == 'a' ? 1 : 0;

    token = strtok_r(NULL, delimiter, &ruleptr);
 
    while (token) {
        
        char *field = strtok_r(NULL, delimiter, &ruleptr);

        switch (token[0]) {

            case ('d'): {

                if (token[1] == 'e')
                    current->dest = strdup(field);
                else {
                    current->domtype = 1;
                    current->domname = strdup(field);
                }

                break;
            }

            case ('s'): {
                current->stubdom = 1;
                break;
            }

            case ('i'): {

                if (token[1] == 'n')
                    current->iface = strdup(field);
                else {
                    size_t boolean_length = strlen(token) + strlen(field) + 1;
                    char *boolean_rule = malloc(sizeof(char) * boolean_length); 
                    snprintf(boolean_rule, boolean_length - 1, "%s %s",
                             token, field);
                    current->if_bool = boolean_rule; 
                    token = strtok_r(NULL, delimiter, &ruleptr);
                    current->if_bool_flag = token[0] == 't' ? 1 : 0;
                }
                
                break;
            }

            case ('p'): {
                current->path = strdup(field);                
                break;
            }

            case ('m'): {
                current->member = strdup(field);
                break;
            }

            default:
                // the 4(?) single cases...
                // "deny all"
                // "allow all"
                // the above, is just the policy bit-field set with all others
                // as null...this will carry-over that blanket policy
                // when testing a request against all standing rules...
                // "deny out-any"
                // "allow out-any"
                // filter warning for to not log these
                DBUS_BROKER_WARNING("Unrecognized Rule-Token: %s", token);
                break;
        }

        token = strtok_r(NULL, delimiter, &ruleptr);
    }

    printf("Rule-Policy: %d\n", current->policy);
    printf("Stubdom    : %d\n", current->stubdom);
    printf("Domtype    : %d\n", current->domtype);
    printf("If-bool    : %d\n", current->if_bool_flag);
    printf("Destination: ");
    if (current->dest)
        printf("%s\n", current->dest);
    else
        printf("None\n");
    printf("Path       : ");
    if (current->path)
        printf("%s\n", current->path);
    else
        printf("None\n");
    printf("Interface  : ");
    if (current->iface)
        printf("%s\n", current->iface);
    else
        printf("None\n");
    printf("Method     : ");
    if (current->member)
        printf("%s\n", current->member);
    else
        printf("None\n");
    printf("Condition  : ");
    if (current->if_bool)
        printf("%s\n", current->if_bool);
    else
        printf("None\n");
    printf("Domname    : ");
    if (current->domname)
        printf("%s\n", current->domname);
    else
        printf("None\n");
     
    return current;
}

// Have this re-used for policy-file aswell...
// db-rule-query is making a dbus-request and spitting out the string of next
// rule found 
int get_rules(DBusConnection *conn, struct rules *domain_rules)
{
    int rule_count = 0;
    char *rule = NULL;
    domain_rules->rule_list = NULL;

    do { 
        domain_rules->rule_list = realloc(domain_rules->rule_list, 
                                          sizeof(struct rule *) * 
                                                (rule_count + 1)); 
        rule = db_rule_query(conn, domain_rules->uuid, rule_count);
        
        if (rule) {
            struct rule *current = create_rule(rule);
            domain_rules->rule_list[rule_count++] = current;
        } 

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

struct rules *get_etc_rules(const char *rule_filename)
{
    struct stat policy_stat;
    if (stat(rule_filename, &policy_stat) < 0)
        return NULL;

    size_t policy_size = policy_stat.st_size;
    char *policy = malloc(sizeof(char) * policy_size + 1);

    int policy_fd = open(rule_filename, O_RDONLY);

    int rbytes = read(policy_fd, policy, policy_size);
    close(policy_fd);

    policy[rbytes] = '\0';
    char *newline = "\n";
    char *fileptr;
    char *rule_token = strtok_r(policy, newline, &fileptr);

    struct rules *etc_rules = malloc(sizeof(struct rules));
    etc_rules->rule_list = NULL;

    int idx = 0;

    while (rule_token) {
        if (isalpha(rule_token[0])) {
            etc_rules->rule_list = realloc(etc_rules->rule_list, 
                                           sizeof(struct rule *) * (idx + 1));
            char *line = strdup(rule_token);
            etc_rules->rule_list[idx++] = create_rule(line);
        }

        rule_token = strtok_r(NULL, newline, &fileptr);
    }

    etc_rules->count = idx;
    return etc_rules;
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

    dbus_policy->etc_rules = get_etc_rules(rule_filename);

    return dbus_policy;
}

