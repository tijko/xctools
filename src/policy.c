#include "../rpc-broker.h"


static inline struct rule *create_rule(char *rule)
{
    struct rule *current = malloc(sizeof *current);
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
                else 
                    current->domname = strdup(field);

                break;
            }

            case ('s'): 
                current->stubdom = 1;
                break;

            case ('i'): {

                if (token[1] == 'n')
                    current->interface = strdup(field);
                else {
                    current->if_bool = strdup(field); 
                    token = strtok_r(NULL, delimiter, &ruleptr);
                    current->if_bool_flag = token[0] == 't' ? 1 : 0;
                }
                
                break;
            }

            case ('p'): 
                current->path = strdup(field);                
                break;

            case ('m'): 
                current->member = strdup(field);
                break;

			case ('a'):
				break;

            default:
                DBUS_BROKER_WARNING("Unrecognized Rule-Token: %s", token);
                // free rule fun
                free(current);
                current = NULL;
                break;
        }

        token = strtok_r(NULL, delimiter, &ruleptr);
    }

    return current;
}

static inline void get_rules(DBusConnection *conn, struct rules *domain_rules)
{
    for (int rule_idx=0; rule_idx < MAX_RULES; rule_idx++) {

        char *arg;
        DBUS_REQ_ARG(arg, "/vm/%s/rpc-firewall-rules/%d", 
                     domain_rules->uuid, rule_idx);

        char *rulestring = db_query(conn, arg);

        if (!rule)
            break;

        struct rule *policy_rule = create_rule(rulestring); 

        if (!policy_rule)
            break;

        domain_rules->rule_list[rule_idx] = policy_rule;
        domain_rules->count++;
    } 
}

struct policy *build_policy(const char *rule_filename)
{
    DBusConnection *conn = create_dbus_connection();
   
    if (!conn) 
        return NULL;

    struct dbus_message dmsg;

    dbus_default(&dmsg);
    dmsg.member = DBUS_LIST;
    dmsg.args[0] = (void *) DBUS_VM_PATH;

    DBusMessage *vms = make_dbus_call(conn, &dmsg);

    if (dbus_message_get_type(vms) == DBUS_MESSAGE_TYPE_ERROR) {
        DBUS_BROKER_WARNING("<No policy in place> %s", "");
        return NULL;
    }

    struct policy *dbus_policy = calloc(1, sizeof *dbus_policy);

    DBusMessageIter iter, sub;
    dbus_message_iter_init(vms, &iter);
    dbus_message_iter_recurse(&iter, &sub); 

    for (int dom_idx=0; 
             dbus_message_iter_get_arg_type(&sub) != DBUS_TYPE_INVALID;
             dom_idx++) {

        void *arg = malloc(sizeof(char) * VM_UUID_LEN);
        dbus_message_iter_get_basic(&sub, &arg);

        dbus_policy->domain_rules[dom_idx] = malloc(sizeof(struct rules));
        struct rules *current = dbus_policy->domain_rules[dom_idx];
        current->uuid = arg;
        current->domid = strtol(arg + DOMID_SECTION, NULL, 10);

        get_rules(conn, current);

        dbus_message_iter_next(&sub);
        dbus_policy->vm_number++;
    }

    // XXX
    //dbus_policy->etc_rules = get_etc_rules(rule_filename);
    return dbus_policy;
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

    free(policy_rules);
}

void free_policy(struct policy *dbus_policy)
{
    free(dbus_policy);
}

// Etc specific structure...
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

    int idx = 0;
/*
    while (rule_token) {
        if (isalpha(rule_token[0])) {
            char *line = strdup(rule_token);
            // re-defined...
            //create_rule(line);
            //if (current)
            //    etc_rules->rule_list[idx++] = current; 
        }

        rule_token = strtok_r(NULL, newline, &fileptr);
    }
*/
    etc_rules->count = idx;
    return etc_rules;
}

