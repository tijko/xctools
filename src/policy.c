#include "../rpc-broker.h"


static inline void create_rule(struct rule *current, char *rule)
{
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
                    current->destination = strdup(field);
                else 
                    current->domtype = strdup(field);

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
}

static inline void get_rules(DBusConnection *conn, struct domain_policy *dom)
{
    for (int rule_idx=0; rule_idx < MAX_RULES; rule_idx++) {

        char *arg;
        DBUS_REQ_ARG(arg, "/vm/%s/rpc-firewall-rules/%d", 
                     dom->uuid, rule_idx);

        char *rulestring = db_query(conn, arg);

        if (!rulestring)
            break;

        struct rule *policy_rule = &(dom->rules[rule_idx]);
        create_rule(policy_rule, rulestring); 

        dom->count++;
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

        struct domain_policy *current = &(dbus_policy->domains[dom_idx]);
        current->uuid = arg;
        current->domid = strtol(arg + DOMID_SECTION, NULL, 10);

        get_rules(conn, current);

        dbus_message_iter_next(&sub);
        dbus_policy->vm_number++;
    }

    // XXX
    //dbus_policy->etc = get_etc_rules(rule_filename);
    return dbus_policy;
}

void free_policy(struct policy *dbus_policy)
{
    free(dbus_policy);
}

// Etc specific structure...
struct etc_policy *get_etc_policy(const char *rule_filename)
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
    return NULL;
/*
    char *newline = "\n";
    char *fileptr;
    char *rule_token = strtok_r(policy, newline, &fileptr);

    struct etc_policy *etc = malloc(sizeof(struct etc_policy));

    int idx = 0;
    while (rule_token) {
        if (isalpha(rule_token[0])) {
            char *line = strdup(rule_token);
            // re-defined...
            //create_rule(line);
            //if (current)
            //    etc->rules[idx++] = current; 
        }

        rule_token = strtok_r(NULL, newline, &fileptr);
    }
    etc->count = idx;
    return etc;
*/
}

