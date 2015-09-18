/*
 * db-helper.c
 *
 * Provide an interface for reading and writing to the DB.
 *
 * Copyright (c) 2015 Assured Information Security, Inc.
 *
 * Author:
 * Jennifer Temkin <temkinj@ainfosec.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "rpcgen/db_client.h"
#include "project.h"
#include "xcpmd.h"
#include "db-helper.h"

/**
 * This file contains functions for reading from and writing to the DB, both
 * low-level and high-level. Many of these allocate memory or have other side
 * effects, so be careful.
 *
 * Variables are implicitly written to the DB when accessed through the functions
 * exposed in db-helper.h, but rules must be written to the DB explicitly.
 *
 * Since variables will generally be read often and written rarely, and DB
 * access is relatively expensive, a write-through cache has been implemented.
 * Cache incoherence is avoided entirely if variables are accessed through the
 * methods in db-helper.h, but if the DB is modified through another pathway
 * (db-rm, for example), the cache can be reset with the load_policy_from_db RPC.
 *
 * Cached variables cannot be deleted if they are referred to by any rules, but
 * they can be overwritten at any time, provided there are no type conflicts.
 * Typing is strictly enforced; a variable cannot be overwritten if the new value
 * does not match its current type.
 * See rules.h for more on argument types.
 *
 * Functions for touching the variable cache are defined here, but its global
 * variable, db_vars, is set up and torn down in rules.c.
 */

//Function prototypes
static void db_write(char * path, char * value);
static char * db_read(char * path);
static void db_rm(char * path);
static char * dump_db_path(char * path);

static void each_leaf(cJSON * node, void (leaf_func(cJSON * node, void * data)), void * data);
static void _write_db_node(cJSON * this_node, void * data_ptr);

static bool each_sibling(char * path, bool (func(cJSON * node, void * data)), void * data);
static bool _sibling_parse_vars(cJSON * node, void * data_ptr);

static char * path_of_node(cJSON * node, cJSON * needle);

static struct arg_node get_db_var(char * var_name);
static void write_db_var(char * name, enum arg_type type, union arg_u value);
static void delete_db_var(char * var_name);
static void delete_db_vars();

static char ** cjson_rule_to_parseable(cJSON * jrule);
static cJSON * rule_to_cjson(struct rule * rule);

static struct db_var * cache_db_var(char * name, enum arg_type type, union arg_u value);
static int uncache_db_var(char * name);


//Container for data passed to _write_db_node.
struct _write_db_node_data {
    cJSON * root;
    char * path_prefix;
};


//Write a value to the specified DB path.
static void db_write(char * path, char * value) {

    com_citrix_xenclient_db_write_(xcdbus_conn, DB_SERVICE, DB_PATH, path, value);
}


//Allocates memory!
//Gets the value of the specified DB key.
//The string returned should be freed.
static char * db_read(char * path) {

    char * string;
    com_citrix_xenclient_db_read_(xcdbus_conn, DB_SERVICE, DB_PATH, path, &string);
    return string;
}


//Remove the specified DB key.
static void db_rm(char * path) {

    com_citrix_xenclient_db_rm_(xcdbus_conn, DB_SERVICE, DB_PATH, path);
}


//Allocates memory!
//Dump the DB contents at the given path into a string.
//The string returned should be freed.
static char * dump_db_path(char * path) {

    char * string;
    com_citrix_xenclient_db_dump_(xcdbus_conn, DB_SERVICE, DB_PATH, path, &string);
    return string;
}


//Writes a variable to the DB. Does not modify the internal cache.
static void write_db_var(char * name, enum arg_type type, union arg_u value) {

    char * var;
    char * path;

    var = arg_to_string(type, value);
    path = safe_sprintf("%s/%s", DB_VAR_MAP_PATH, name);
    db_write(path, var);
    free(var);
    free(path);
}


//May allocate memory!
//Reads a variable from the DB and parses it into an arg_node.
//Does not modify the internal cache.
//If the argument is a string, that string is malloc'd.
static struct arg_node get_db_var(char * var_name) {

    struct arg_node arg;
    char * path;
    char *var_value, *var_string;
    char * error;

    path = safe_sprintf("%s%s", DB_VAR_MAP_PATH, var_name);
    var_value = db_read(path);

    if (var_value == NULL) {
        arg.type = ARG_NONE;
        arg.arg.i = 0;
        return arg;
    }

    if (var_value[0] != '\0') {
        var_string = safe_sprintf("%s(%s)", var_name, var_value);

        if (!parse_arg(var_string, &arg, &error)) {
            xcpmd_log(LOG_WARNING, "Var %s's DB entry (%s) was malformed: %s\n", var_name, var_value, error);
            arg.type = ARG_NONE;
            arg.arg.i = 0;
            free(error);
        }

        free(var_string);
    }
    else {
        arg.type = ARG_NONE;
        arg.arg.i = 0;
    }

    free(var_value);
    free(path);

    return arg;
}


//Deletes the specified variable from the DB. Does not modify the internal cache.
static void delete_db_var(char * var_name) {

    char * path;
    path = safe_sprintf("%s%s", DB_VAR_MAP_PATH, var_name);
    db_rm(path);
    free(path);
}


//Deletes all variables in the DB. Does not modify the internal cache.
static void delete_db_vars() {

    db_rm(DB_VAR_MAP_PATH);
}


//Performs func() for each sibling under a DB path.
static bool each_sibling(char * path, bool (func(cJSON * node, void * data)), void * data) {

    cJSON *jroot, *jsib;
    char * json, *ep;
    int i, num_vars;
    bool success, total_success;
    bool rpcproxy = false;

    while(!rpcproxy) {
        json = dump_db_path(path);
        if(json == NULL) {
            xcpmd_log(LOG_WARNING, "Error opening DB node %s - received null response\n", path);
            return false;
        }
        if(json[0] == '\0') {
            free(json);
            return true;
        }

        jroot = cJSON_Parse(json);
        if (jroot == NULL) {

            //If rpc-proxy is queried before it's completely started up, it'll
            //return non-parseable gibberish. cJSON returns NULL on both a 
            //memory error and if the string is not parseable, but in the 
            //second case, it sets an error pointer to the first non-parseable
            //character it finds. If ep is within the JSON string, then we're
            //fairly sure we have gibberish.
            ep = (char *)cJSON_GetErrorPtr();
            if (json <= ep && strchr(json, '\0') >= ep) {
                xcpmd_log(LOG_WARNING, "Invalid DB string: %s", json);
                rpcproxy = false;
                free(json);

                //Wait for a while--maybe rpc-proxy will be there when we wake up.
                sleep(1);
                continue;
            }
            else {
                xcpmd_log(LOG_WARNING, "Error opening DB node %s - out of memory\n", path);//FIXME: 
                free(json);
                return false;
            }
        }
        free(json);
        rpcproxy = true;
    }

    //Path is a leaf.
    if (jroot->type != cJSON_Object && jroot->type != cJSON_Array) {
        cJSON_Delete(jroot);
        return true;
    }

    num_vars = cJSON_GetArraySize(jroot);

    total_success = true;
    for (i=0; i < num_vars; ++i) {
        jsib = cJSON_GetArrayItem(jroot, i);
        success = func(jsib, data);
        total_success = success && total_success;
    }

    cJSON_Delete(jroot);

    return total_success;
}


//Recursively walks a cJSON tree (depth-first) and calls func() on each leaf.
//For the initial call, node should be the root of the tree to walk.
static void each_leaf(cJSON * node, void (func(cJSON * node, void * data)), void * data) {

    if (node == NULL)
        return;

    if (node->child == NULL) { //we're a leaf
        func(node, data);
    }

    each_leaf(node->child, func, data);
    each_leaf(node->next, func, data);
}


//Parse and cache all DB variables. Requires an initialized parse_data struct.
bool parse_db_vars(struct parse_data * parse_data) {

    return each_sibling(DB_VAR_MAP_PATH, _sibling_parse_vars, (void *)parse_data);
}


//Component of parse_db_vars().
//Parses a variable found in the given cJSON node.
static bool _sibling_parse_vars(cJSON * node, void * data_ptr) {

    char * var_string;

    struct parse_data * data = (struct parse_data *)data_ptr;

    if (node == NULL) {
        xcpmd_log(LOG_WARNING, "DB node was null");
        return false;
    }
    if (node->type != cJSON_String) {
        xcpmd_log(LOG_WARNING, "DB node was not a string");
        return false;
    }
    if (node->string == NULL || node->valuestring == NULL) {
        xcpmd_log(LOG_WARNING, "DB node is malformed");
        return false;
    }

    var_string = safe_sprintf("%s(%s)", node->string, node->valuestring);
    if (parse_var_persistent(data, var_string)) {
        free(var_string);
        return true;
    }
    else {
        xcpmd_log(LOG_WARNING, "Error parsing var string %s: %s", var_string, extract_parse_error(data));
        free(var_string);
        return false;
    }
}


//Write the specified rule to the DB. Does not modify the internal cache.
void write_db_rule(struct rule * rule) {

    cJSON * cj;
    struct _write_db_node_data data;

    cj = rule_to_cjson(rule);

    data.root = cj;
    data.path_prefix = DB_RULE_PATH;

    each_leaf(cj, _write_db_node, (void *)&data);

    cJSON_Delete(cj);
}


//Component of write_db_rule().
//Writes a node to the DB, respecting its path structure.
static void _write_db_node(cJSON * this_node, void * data_ptr) {

    struct _write_db_node_data * data;
    char * path_suffix, *path;

    data = (struct _write_db_node_data *)data_ptr;

    path_suffix = path_of_node(data->root, this_node);
    path = safe_sprintf("%s%s", data->path_prefix, path_suffix);

    //xcpmd_log(LOG_DEBUG, "Writing %s to %s.\n", this_node->valuestring, path);

    db_write(path, this_node->valuestring);
    free(path);
    free(path_suffix);
}


//Write all rules to the DB.
void write_db_rules() {

    struct rule * rule;

    list_for_each_entry(rule, &rules.list, list) {
        write_db_rule(rule);
    }
}


//Deletes the specified rule from the DB. Does not modify the internal rule list.
void delete_db_rule(char * rule_name) {

    char * path;
    path = safe_sprintf("%s%s", DB_RULE_PATH, rule_name);
    db_rm(path);
    free(path);
}


//Deletes all rules in the DB. Does not modify the internal rule list.
void delete_db_rules() {

    db_rm(DB_RULE_PATH);
}


//Parses rules from the DB and adds them to the internal rule list.
bool parse_db_rules(struct parse_data * data) {

    cJSON *jroot, *jrule;
    char ** rule_arr;
    char *json, *name, *conditions, *actions, *undos;
    char * err;
    int num_rules, i, j;

    json = dump_db_path(DB_RULE_PATH);

    if (strcmp(json, "") == 0) {
        xcpmd_log(LOG_DEBUG, "DB rules node is empty\n");
        free(json);
        return true;
    }

    jroot = cJSON_Parse(json);
    if (jroot == NULL) {
        xcpmd_log(LOG_DEBUG, "Error parsing DB rules - memory error\n");
        free(json);
        return false;
    }
    free(json);

    if (jroot->type != cJSON_Object) {
        xcpmd_log(LOG_DEBUG, "Error parsing DB rules - %s is malformed", DB_RULE_PATH);
        cJSON_Delete(jroot);
        return false;
    }

    num_rules = cJSON_GetArraySize(jroot);
    for (i=0; i < num_rules; ++i) {

        jrule = cJSON_GetArrayItem(jroot, i);

        rule_arr = cjson_rule_to_parseable(jrule);
        if (rule_arr == NULL) {
            xcpmd_log(LOG_WARNING, "Error parsing DB rule - rule %d is malformed", i);
            continue;
        }
        name = rule_arr[0];
        conditions = rule_arr[1];
        actions = rule_arr[2];
        undos = rule_arr[3];

        if (!parse_rule_persistent(data, name, conditions, actions, undos)) {
            err = extract_parse_error(data);
            xcpmd_log(LOG_WARNING, "Error reading DB rule %s - %s", jrule->string == NULL ? "(null)" : jrule->string, err);
        }

        for (j=0; j < 4; ++j)
            free(rule_arr[j]);
        free(rule_arr);
    }

    cJSON_Delete(jroot);

    return true;
}


//Allocates memory!
//Converts a rule in a tree of cJSON nodes into a set of strings that the parser can handle.
//Returns an array of name, conditions, actions, and undos as parseable strings.
//Allocates memory for both the array and the strings themselves.
static char ** cjson_rule_to_parseable(cJSON * jrule) {

    char *name, *conditions, *actions, *undos;
    char ** string_array;
    int i, j, num_entries, num_args;
    cJSON *jconditions, *jcond, *jargs, *jarg, *jactions, *jact, *jundos, *jundo, *jtype, *jinverted;
    bool has_undos;
    char *str = NULL;

    /*
     * A rule looks like this in the DB:
     *   rules:
     *       rule_name:
     *           conditions:
     *               0:
     *                   type: "whileUsingBatt"
     *                   is_inverted: "f"
     *                   args: ""
     *               1:
     *                   type: "whileBattLessThan"
     *                   is_inverted: "f"
     *                   args:
     *                       0: "50"
     *           actions:
     *               0:
     *                   type: "logString"
     *                   args:
     *                       0: "\"battery is less than 50%!\""
     *           undos: ""
     *
     * Whereas the parser understands rules in a four-string form, like this:
     *   name:       "rule_name"
     *   conditions: "whileUsingBatt() whileBattLessThan(50)"
     *   actions:    "logString(\"battery is less than 50%!\")"
     *   undos:      ""
     *
     * So we convert the DB structure into a cJSON tree, and walk it to get the
     * information we need.
     */

    //There's no guarantee that a DB node (and the cJSON generated from it) will
    //be properly formatted, so a lot of error checking is necessary.

    //Get the rule name.
    if (jrule->string == NULL) {
        xcpmd_log(LOG_WARNING, "Rule with no name detected");
        return NULL;
    }
    name = clone_string(jrule->string);

    //Get the conditions.
    jconditions = cJSON_GetObjectItem(jrule, "conditions");
    if (jconditions == NULL) {
        xcpmd_log(LOG_WARNING, "Rule %s has no conditions", name);
        goto free_name;
    }
    if (jconditions->type != cJSON_Object) {
        xcpmd_log(LOG_WARNING, "Rule %s's conditions are malformed", name);
        goto free_name;
    }

    //Build the condition string, separating each condition with spaces.
    num_entries = cJSON_GetArraySize(jconditions);
    for (i = 0; i < num_entries; ++i) {

        jcond = cJSON_GetArrayItem(jconditions, i);

        //Is the condition inverted?
        jinverted = cJSON_GetObjectItem(jcond, "is_inverted");
        if (jinverted == NULL || jinverted->valuestring == NULL) {
            xcpmd_log(LOG_WARNING, "Condition in rule %s is missing is_inverted.", name);
            goto free_name;
        }
        if (!strcmp(jinverted->valuestring, "true"))
            safe_str_append(&str, "!");

        //What's its type?
        jtype = cJSON_GetObjectItem(jcond, "type");
        if (jtype == NULL || jtype->valuestring == NULL) {
            xcpmd_log(LOG_WARNING, "Condition in rule %s is missing type.", name);
            goto free_name;
        }
        safe_str_append(&str, "%s(", jtype->valuestring);

        //Get its arguments, if it has any.
        jargs = cJSON_GetObjectItem(jcond, "args");
        if (jargs != NULL) {

            if (!((jargs->type == cJSON_Object) || (jargs->type == cJSON_String))) {
                xcpmd_log(LOG_WARNING, "Args of condition %s in rule %s is malformed", jtype->valuestring, name);
                goto free_name;
            }

            num_args = cJSON_GetArraySize(jargs);
            for (j = 0; j < num_args; ++j) {

                jarg = cJSON_GetArrayItem(jargs, j);

                if (jarg->valuestring == NULL) {
                    xcpmd_log(LOG_WARNING, "Empty arg in condition %s in rule %s.\n", jtype->valuestring, name);
                    goto free_name;
                }

                if (j == (num_args - 1))
                    safe_str_append(&str, "%s", jarg->valuestring);
                else
                    safe_str_append(&str, "%s ", jarg->valuestring);
            }
        }

        //Terminate with a space if this isn't the final condition.
        if (i == (num_entries - 1))
            safe_str_append(&str, ")");
        else
            safe_str_append(&str, ") ");
    }
    conditions = str;
    str = NULL;

    //Now do the same thing to get the action string.
    jactions = cJSON_GetObjectItem(jrule, "actions");
    if (jactions == NULL) {
        xcpmd_log(LOG_WARNING, "Rule %s has no actions", name);
        goto free_conditions;
    }
    if (jactions->type != cJSON_Object) {
        xcpmd_log(LOG_WARNING, "Rule %s's actions are malformed", name);
        goto free_conditions;
    }

    //Glob those actions into a space-separated string.
    num_entries = cJSON_GetArraySize(jactions);
    for (i = 0; i < num_entries; ++i) {

        jact = cJSON_GetArrayItem(jactions, i);

        //Get the action type.
        jtype = cJSON_GetObjectItem(jact, "type");
        if (jtype == NULL || jtype->valuestring == NULL) {
            xcpmd_log(LOG_WARNING, "Action in rule %s is missing type.", name);
            goto free_conditions;
        }
        safe_str_append(&str, "%s(", jtype->valuestring);

        //Get its args, if it has any.
        jargs = cJSON_GetObjectItem(jact, "args");
        if (jargs != NULL) {

            if (!((jargs->type == cJSON_Object) || (jargs->type == cJSON_String))) {
                xcpmd_log(LOG_WARNING, "Args of action %s in rule %s is malformed", jtype->valuestring, name);
                goto free_conditions;
            }
            num_args = cJSON_GetArraySize(jargs);
            for (j = 0; j < num_args; ++j) {

                jarg = cJSON_GetArrayItem(jargs, j);

                if (j == (num_args - 1))
                    safe_str_append(&str, "%s", jarg->valuestring);
                else
                    safe_str_append(&str, "%s ", jarg->valuestring);
            }
        }
        //Terminate with a space if this isn't the final action.
        if (i == (num_entries - 1))
            safe_str_append(&str, ")");
        else
            safe_str_append(&str, ") ");
    }
    actions = str;
    str = NULL;

    //Finally, build the undo actions string.
    jundos = cJSON_GetObjectItem(jrule, "undos");
    if (jundos != NULL) {
        if (jundos->type == cJSON_String) {
            if (jundos->valuestring == NULL || jundos->valuestring[0] == '\0') {
                has_undos = false;
            }
            else {
                xcpmd_log(LOG_WARNING, "Rule %s's undos are malformed", name);
                goto free_actions;
            }
        }
        else if (jundos->type != cJSON_Object) {
            xcpmd_log(LOG_WARNING, "Rule %s's undos are malformed", name);
            goto free_actions;
        }
        else {
            has_undos = true;
        }

        //Undo actions are not required, so this string may be empty.
        //Otherwise, this section is identical to the previous one.
        if (has_undos == true) {

            num_entries = cJSON_GetArraySize(jundos);
            for (i = 0; i < num_entries; ++i) {

                jundo = cJSON_GetArrayItem(jundos, i);

                jtype = cJSON_GetObjectItem(jundo, "type");
                if (jtype == NULL || jtype->valuestring == NULL) {
                    xcpmd_log(LOG_WARNING, "Undo in rule %s is missing type.", name);
                    goto free_actions;
                }
                safe_str_append(&str, "%s(", jtype->valuestring);

                jargs = cJSON_GetObjectItem(jundo, "args");
                if (jargs != NULL) {

                    if (!((jargs->type == cJSON_Object) || (jargs->type == cJSON_String))) {
                        xcpmd_log(LOG_WARNING, "Args of undo %s in rule %s is malformed", jtype->valuestring, name);
                        goto free_actions;
                    }
                    num_args = cJSON_GetArraySize(jargs);
                    for (j = 0; j < num_args; ++j) {

                        jarg = cJSON_GetArrayItem(jargs, j);

                        if (j == (num_args - 1))
                            safe_str_append(&str, "%s", jarg->valuestring);
                        else
                            safe_str_append(&str, "%s, ", jarg->valuestring);
                    }
                }

                if (i == (num_entries - 1))
                    safe_str_append(&str, ")");
                else
                    safe_str_append(&str, ") ");
            }
        }
    }
    undos = str;

    //Finally, plunk the strings into a string array.
    string_array = (char **)malloc(4 * sizeof(char *));
    string_array[0] = name;
    string_array[1] = conditions;
    string_array[2] = actions;
    string_array[3] = undos;

    return string_array;

//Failure modes - free anything allocated up to the point of failure.
free_actions:
    free(actions);
free_conditions:
    free(conditions);
free_name:
    free(name);

    if (str != NULL) {
        free(str);
    }

    return NULL;
}


//Allocates memory!
//Converts a rule to a cJSON tree.
//This tree is dynamically allocated and must be destroyed with cJSON_Delete().
static cJSON * rule_to_cjson(struct rule * rule) {

    char index_string[32];
    char * arg_string;
    int index, arg_index;
    struct condition * cond;
    struct action * act;
    struct arg_node * arg;
    cJSON *jroot, *jrule, *jconditions, *jcond, *jargs, *jactions, *jact, *jundos, *jundo;
    char * ret;

    //Roots can't have names, so we create a blank node for our root and build
    //the rule on top of it.
    jroot = cJSON_CreateObject();
    cJSON_AddItemToObject(jroot, rule->id, jrule=cJSON_CreateObject());

    //Add the rule's conditions.
    cJSON_AddItemToObject(jrule, "conditions", jconditions=cJSON_CreateObject());
    index = 0;
    list_for_each_entry(cond, &rule->conditions.list, list) {
        snprintf(index_string, 32, "%d", index);
        cJSON_AddItemToObject(jconditions, index_string, jcond=cJSON_CreateObject());
        cJSON_AddStringToObject(jcond, "type", cond->type->name);
        cJSON_AddStringToObject(jcond, "is_inverted", (cond->is_inverted ? "true" : "false"));
        cJSON_AddItemToObject(jcond, "args", jargs=cJSON_CreateObject());

        //Add the condition's arguments, if it has any.
        arg_index = 0;
        list_for_each_entry(arg, &cond->args.list, list) {
            snprintf(index_string, 32, "%d", arg_index);
            arg_string = arg_to_string(arg->type, arg->arg);
            cJSON_AddStringToObject(jargs, index_string, arg_string);
            free(arg_string);
            ++arg_index;
        }

        ++index;
    }

    //Add the rule's actions.
    cJSON_AddItemToObject(jrule, "actions", jactions=cJSON_CreateObject());
    index = 0;
    list_for_each_entry(act, &rule->actions.list, list) {
        snprintf(index_string, 32, "%d", index);
        cJSON_AddItemToObject(jactions, index_string, jact=cJSON_CreateObject());
        cJSON_AddStringToObject(jact, "type", act->type->name);
        cJSON_AddItemToObject(jact, "args", jargs=cJSON_CreateObject());

        //And its arguments.
        arg_index = 0;
        list_for_each_entry(arg, &act->args.list, list) {
            snprintf(index_string, 32, "%d", arg_index);
            arg_string = arg_to_string(arg->type, arg->arg);
            cJSON_AddStringToObject(jargs, index_string, arg_string);
            free(arg_string);
            ++arg_index;
        }

        ++index;
    }

    //Add the undo actions, if any.
    cJSON_AddItemToObject(jrule, "undos", jundos=cJSON_CreateObject());
    index = 0;
    list_for_each_entry(act, &rule->undos.list, list) {
        snprintf(index_string, 32, "%d", index);
        cJSON_AddItemToObject(jundos, index_string, jundo=cJSON_CreateObject());
        cJSON_AddStringToObject(jundo, "type", act->type->name);
        cJSON_AddItemToObject(jundo, "args", jargs=cJSON_CreateObject());

        //And arguments.
        arg_index = 0;
        list_for_each_entry(arg, &act->args.list, list) {
            snprintf(index_string, 32, "%d", arg_index);
            arg_string = arg_to_string(arg->type, arg->arg);
            cJSON_AddStringToObject(jargs, index_string, arg_string);
            free(arg_string);
            ++arg_index;
        }

        ++index;
    }

    //Discard the blank root.
    cJSON_DetachItemFromObject(jroot, rule->id);
    cJSON_Delete(jroot);

    return jrule;
}


//Allocates memory!
//Gets the DB path to the specified cJSON node.
//For the initial call, node should be the root of the tree to search.
//Needle is the node to find the path to.
//Returns the path from the root to the needle, or null if the search fails.
//The string returned must be freed.
static char * path_of_node(cJSON * node, cJSON * needle) {

    //Recursively walk the tree and build/tear down the path as you go.

    static char * pathbuild = NULL;
    static char * ret = NULL;
    static int depth = 0;
    char * path;
    char * ptr;

    char buffer[1024];

    //If ret is set, we've found the needle. Don't recurse further.
    if (ret != NULL) {
        return NULL;
    }

    //If this was somehow called on a null node, return.
    if (node == NULL) {
        return NULL;
    }

    //Add this node to the path
    if (node->string != NULL) {
        safe_str_append(&pathbuild, "/%s", node->string);
    }

    //If we've found the needle, set ret and stop traversing the tree.
    if (node == needle) {
        ret = clone_string(pathbuild);
    }
    else {

        //Try this node's children.
        ++depth;
        path_of_node(node->child, needle);
        --depth;

        //Remove this node from the path
        ptr = strchr(pathbuild, '\0');
        while (ptr > pathbuild && ptr != 0 && *ptr != '/') {
            *ptr = '\0';
            ptr -= sizeof(char);
        }
        *ptr = '\0';

        //Try this node's siblings.
        path_of_node(node->next, needle);

    }
    //If we're the root node, reset all static variables and return the path.
    if (depth == 0) {
        path = ret;
        ret = NULL;
        free(pathbuild);
        pathbuild = NULL;

        //xcpmd_log(LOG_DEBUG, "Path of %s is %s", needle->string, path);
        return path;
    }

    return NULL;
}


//Allocates memory!
//Adds a variable, transparently caching it and writing back to the DB.
//If the variable already exists, and there are no type conflicts, its previous
//value will be overwritten.
//Modifies the internal cache.
//Writes error messages to *parse_error.
//May try to free existing strings in *parse_error--don't supply the address of
//a non-malloc'd string!
struct db_var * add_var(char * name, enum arg_type type, union arg_u value, char ** parse_error) {

    struct db_var * var = lookup_var(name);

    //A var with this name exists already. Check the type.
    if (var != NULL) {
        if (var->value.type != type) {
            safe_str_append(parse_error, "Attempted to overwrite variable %s, but aborted due to type mismatch (was %s, tried %s)",
                    name, arg_type_to_string((char)var->value.type), arg_type_to_string((char)type));
            return NULL;
        }
        if (!((var->value.type == ARG_STR && !strcmp(var->value.arg.str, value.str)) || !memcmp(&var->value.arg, &value, sizeof(union arg_u)))) {
            safe_str_append(parse_error, "Variable %s already exists--overwriting", name);
        }
        else {
            return var;
        }

        if(type == ARG_STR) {
            free(var->value.arg.str);
            var->value.arg.str = clone_string(value.str);
        }
        else {
            var->value.arg = value;
        }
    }
    else {
        var = cache_db_var(name, type, value);
    }

    if (var != NULL) {
        write_db_var(var->name, var->value.type, var->value.arg);
    }

    return var;
}


//Looks up a db_var based on its name, and caches it if it's not already in the
//cache. Returns null if the search fails.
struct db_var * lookup_var(char * name) {

    struct db_var * tmp_var, *found_var = NULL;
    struct arg_node tmp_arg;

    //Check if the var is cached.
    list_for_each_entry(tmp_var, &(db_vars.list), list) {
        if (strcmp(tmp_var->name, name) == 0) {
            found_var = tmp_var;
            break;
        }
    }

    //If not, look it up in the DB.
    if (found_var == NULL) {
        tmp_arg = get_db_var(name);

        //If it exists, add it to the cache.
        if (tmp_arg.type != ARG_NONE) {
            found_var = cache_db_var(name, tmp_arg.type, tmp_arg.arg);

            //get_db_var() allocs strings, so free.
            if (tmp_arg.type == ARG_STR) {
                free(tmp_arg.arg.str);
            }
        }
    }

    return found_var;
}


//Convenience function to skip the db_var struct.
struct arg_node * resolve_var(char * name) {

    struct db_var * var = lookup_var(name);

    if (var == NULL) {
        return NULL;
    }
    else {
        return &var->value;
    }
}


//Deletes a variable from both the DB and the internal cache.
int delete_var(char * name) {

    if (uncache_db_var(name)) {
        delete_db_var(name);
        return 1;
    }
    else {
        return 0;
    }
}


//Deletes all vars from both the DB and the internal cache.
void delete_vars() {

    delete_cached_vars();
    delete_db_vars();
}


//Allocates memory!
//Adds a variable to the cache. Allocates memory for both the db_var struct and
//any strings that must be copied.
static struct db_var * cache_db_var(char * name, enum arg_type type, union arg_u value) {

    char *string_copy;
    struct db_var * var = (struct db_var *)malloc(sizeof(struct db_var));
    if (var == NULL) {
        xcpmd_log(LOG_ERR, "Failed to allocate memory\n");
        return NULL;
    }

    var->name = clone_string(name);
    var->value.type = type;
    if (type == ARG_STR) {
        var->value.arg.str = clone_string(value.str);
    }
    else {
        var->value.arg = value;
    }

    var->ref_count = 0;

    list_add_tail(&var->list, &db_vars.list);

    return var;
}


//Removes a variable from the internal cache. Does not modify the DB.
//Fails if the variable is required by any currently loaded rules.
static int uncache_db_var(char * name) {

    struct db_var *tmp_var, *found_var = NULL;

    list_for_each_entry(tmp_var, &(db_vars.list), list) {
        if (strcmp(tmp_var->name, name) == 0) {
            found_var = tmp_var;
            break;
        }
    }

    if (found_var == NULL) {
        return 0;
    }

    if (found_var->ref_count > 0) {
        xcpmd_log(LOG_WARNING, "Attempted to uncache %s, but it is currently in use in %d place%s",
                name, found_var->ref_count, found_var->ref_count == 1 ? "" : "s");
        return 0;
    }

    list_del(&found_var->list);
    if (found_var->value.type == ARG_STR) {
        free(found_var->value.arg.str);
    }
    free(found_var->name);
    free(found_var);
    return 1;
}


//Clears the entire var cache, regardless of refcounts. Does not modify the DB.
void delete_cached_vars() {

    struct list_head *posi, *i;
    struct db_var * tmp_var;

    //Clean up all cached db_vars.
    list_for_each_safe(posi, i, &db_vars.list) {
        tmp_var = list_entry(posi, struct db_var, list);
        list_del(posi);
        free(tmp_var->name);
        if (tmp_var->value.type == ARG_STR) {
            free(tmp_var->value.arg.str);
        }
        free(tmp_var);
    }
}
