/*
 * xcpmd-dbus-server.c
 *
 * Put xcpmd on dbus, implement xcpmd dbus methods and
 * signal power management events.
 *
 * Copyright (c) 2008 Kamala Narasimhan <kamala.narasimhan@citrix.com>
 * Copyright (c) 2011 Ross Philipson <ross.philipson@citrix.com>
 * Copyright (c) 2011 Citrix Systems, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
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

#include "project.h"
#include "xcpmd.h"
#include "parser.h"
#include "db-helper.h"
#include "battery.h"

xcdbus_conn_t *xcdbus_conn = NULL;


//Adds a rule to the internal rule list. Rule is error-checked before being 
//accepted--see validate_rule() in rules.c for error conditions.
//Sample call: dbus-send --system --print-reply --dest=com.citrix.xenclient.xcpmd / com.citrix.xenclient.xcpmd.add_rule string:'rule1' string:'whileUsingBatt()' string:'logString("on battery")' string:'logString("on AC")'
gboolean xcpmd_add_rule(XcpmdObject *this, const char* IN_name, const char* IN_conditions, const char* IN_actions, const char* IN_undo_actions, GError** error) {

    char * parse_error = NULL;
    //xcpmd_log(LOG_DEBUG, "processing rule. name: %s; conditions: %s; actions: %s; undos: %s", IN_name, IN_conditions, IN_actions, IN_undo_actions);

    if (!parse_rule((char *)IN_name, (char *)IN_conditions, (char *)IN_actions, (char *)IN_undo_actions, &parse_error)) {
        xcpmd_log(LOG_WARNING, "%s", parse_error);
        g_set_error(error, DBUS_GERROR, DBUS_GERROR_FAILED, "%s", parse_error);
        free(parse_error);
        return FALSE;
    }
    else {
        xcpmd_log(LOG_INFO, "Added rule %s.\n", IN_name);
        return TRUE;
    }
}


//Removes the named rule from the internal rule list.
gboolean xcpmd_remove_rule(XcpmdObject *this, const char* IN_rule_name, GError** error) {

    struct rule * rule;

    rule = lookup_rule((char *)IN_rule_name);
    if (rule == NULL) {
        g_set_error(error, DBUS_GERROR, DBUS_GERROR_FAILED, "No rule named %s", IN_rule_name);
        return FALSE;
    }

    xcpmd_log(LOG_INFO, "Deleting rule %s.\n", IN_rule_name);
    delete_rule(rule);
    delete_db_rule((char *)IN_rule_name);

    return TRUE;
}


//Loads variables and rules from the named file. See parse_config_from_file()
//in parser.c for file syntax.
gboolean xcpmd_load_policy_from_file(XcpmdObject *this, const char* IN_filename, GError** error) {

    xcpmd_log(LOG_INFO, "Loading policy from file %s.\n", IN_filename);

    if (parse_config_from_file((char *)IN_filename) != 0) {
        g_set_error(error, DBUS_GERROR, DBUS_GERROR_FAILED, "Error parsing config file--check dom0 syslog");
        write_db_rules();
        return FALSE;
    }
    else {
        write_db_rules();
        return TRUE;
    }
}


//Loads variables and rules from the DB.
gboolean xcpmd_load_policy_from_db(XcpmdObject *this, GError** error) {

    xcpmd_log(LOG_INFO, "Reloading policy from DB.\n");
    delete_rules();

    if (parse_config_from_db() != 0) {
        g_set_error(error, DBUS_GERROR, DBUS_GERROR_FAILED, "Error parsing DB policy--check dom0 syslog");
        return FALSE;
    }
    else {
        print_rules();
        return TRUE;
    }
}


//Deletes all rules and variables from both the DB and internal storage.
gboolean xcpmd_clear_policy(XcpmdObject *this, GError** error) {

    xcpmd_log(LOG_INFO, "Clearing policy.\n");
    delete_rules();
    delete_db_rules();
    delete_vars();

    return TRUE;
}


//Deletes all rules from both the DB and internal storage.
gboolean xcpmd_clear_rules(XcpmdObject *this, GError** error) {

    xcpmd_log(LOG_INFO, "Clearing rules.\n");
    delete_rules();
    delete_db_rules();

    return TRUE;
}


//Deletes all variables from both the DB and internal cache. Variables still
//referred to by rules are left alone.
gboolean xcpmd_clear_vars(XcpmdObject *this, GError** error) {

    xcpmd_log(LOG_INFO, "Clearing variables.\n");
    delete_vars();

    return TRUE;
}


//Adds a variable to the DB and internal cache. Type is inferred from string
//format--strings must be in double quotes, booleans are non-quoted t and f,
//numbers with a decimal point are floats, and numbers without are ints.
//TODO: reevaluate rules depending on any modified variables
gboolean xcpmd_add_var(XcpmdObject *this, const char* IN_name, const char* IN_value, GError** error) {

    char * var_string;
    struct db_var * var;
    enum arg_type type;
    union arg_u arg;
    char * parse_error = NULL;

    var_string = safe_sprintf("%s(%s)", IN_name, IN_value);

    if (!parse_var(var_string, &parse_error)) {
        xcpmd_log(LOG_WARNING, "%s", parse_error);
        g_set_error(error, DBUS_GERROR, DBUS_GERROR_FAILED, "%s", parse_error);
        free(var_string);
        free(parse_error);
        return FALSE;
    }
    else {
        xcpmd_log(LOG_INFO, "Added var %s.\n", IN_name);
        free(var_string);
        return TRUE;
    }
}


//Deletes the named variable from the DB and internal cache, unless it is
//referred to by a rule.
gboolean xcpmd_remove_var(XcpmdObject *this, const char* IN_name, GError** error) {

    if (delete_var((char *)IN_name)) {
        xcpmd_log(LOG_INFO, "Deleted var %s.", IN_name);
        return TRUE;
    }
    else {
        xcpmd_log(LOG_WARNING, "Couldn't delete var %s - still referenced by at least one rule.", IN_name);
        g_set_error(error, DBUS_GERROR, DBUS_GERROR_FAILED, "Variable still referenced by at least one rule");
        return FALSE;
    }
}


//Gets a human-readable list of available condition_types and their prototypes.
gboolean xcpmd_get_conditions(XcpmdObject *this, char** *OUT_conditions, GError** error) {

    char ** str_array, ** nt_str_array;
    int num_strings = 0;
    int i;

    num_strings = get_registered_condition_types(&str_array);
    xcpmd_log(LOG_DEBUG, "Number of conditions registered: %d\n", num_strings);

    //Null-terminate the string array.
    nt_str_array = (char **)malloc((num_strings + 1) * sizeof(char *));
    if (nt_str_array == NULL) {
        xcpmd_log(LOG_ERR, "Couldn't allocate memory");

        for (i = 0; i < num_strings; ++i) {
            free(str_array[i]);
        }
        free(str_array);
        return FALSE;
    }
    memcpy(nt_str_array, str_array, (num_strings + 1) * sizeof(char *));
    nt_str_array[num_strings] = NULL;

    *OUT_conditions = nt_str_array;

    return TRUE;
}


//Gets a human-readable list of available action_types and their prototypes.
gboolean xcpmd_get_actions(XcpmdObject *this, char** *OUT_actions, GError** error) {

    char ** str_array, ** nt_str_array;
    int num_strings = 0;
    int i;

    num_strings = get_registered_action_types(&str_array);
    xcpmd_log(LOG_DEBUG, "Number of actions registered: %d\n", num_strings);

    //Null-terminate the string array.
    nt_str_array = (char **)malloc((num_strings + 1) * sizeof(char *));
    if (nt_str_array == NULL) {
        xcpmd_log(LOG_ERR, "Couldn't allocate memory");

        for (i = 0; i < num_strings; ++i) {
            free(str_array[i]);
        }
        free(str_array);
        return FALSE;
    }
    memcpy(nt_str_array, str_array, (num_strings + 1) * sizeof(char *));
    nt_str_array[num_strings] = NULL;

    *OUT_actions = nt_str_array;
    return TRUE;
}


//Gets a human-readable list of the currently loaded rules.
gboolean xcpmd_get_rules(XcpmdObject *this, char** *OUT_rules, GError** error) {

    unsigned int num_rules, i, j;
    char ** rule_strings;
    struct rule * rule;

    num_rules = list_length(&rules.list);
    rule_strings = (char **)malloc((num_rules + 1) * sizeof(char *));
    if (rule_strings == NULL) {
        xcpmd_log(LOG_ERR, "Couldn't allocate memory!");
        g_set_error(error, DBUS_GERROR, DBUS_GERROR_FAILED, "Couldn't allocate memory!");
        return FALSE;
    }

    i = 0;
    list_for_each_entry(rule, &rules.list, list) {
        rule_strings[i] = rule_to_string(rule);
        if (rule_strings[i] == NULL) {
            xcpmd_log(LOG_WARNING, "Couldn't convert rule %d to string!", i);
            g_set_error(error, DBUS_GERROR, DBUS_GERROR_FAILED, "Couldn't convert rule %d to string!", i);
            for (j = 0; j < i; ++j) {
                free(rule_strings[j]);
            }
            free(rule_strings);
            return FALSE;
        }
        ++i;
    }

    //Null-terminate the string array
    rule_strings[num_rules] = NULL;
    *OUT_rules = rule_strings;

    return TRUE;

}


//Gets a human-readable list of the currently loaded variables.
gboolean xcpmd_get_vars(XcpmdObject *this, char** *OUT_vars, GError** error) {

    unsigned int num_vars, i, j;
    char ** var_strings;
    struct db_var * var;
    struct arg_node * arg;
    char * arg_string;

    num_vars = list_length(&db_vars.list);
    var_strings = (char **)malloc((num_vars + 1) * sizeof(char *));
    if (var_strings == NULL) {
        xcpmd_log(LOG_ERR, "Couldn't allocate memory!");
        g_set_error(error, DBUS_GERROR, DBUS_GERROR_FAILED, "Couldn't allocate memory!");
        return FALSE;
    }

    i = 0;
    list_for_each_entry(var, &db_vars.list, list) {
        arg = resolve_var(var->name);
        arg_string = arg_to_string(arg->type, arg->arg);
        var_strings[i] = safe_sprintf("%s(%s)", var->name, arg_string);
        if (var_strings[i] == NULL) {
            xcpmd_log(LOG_WARNING, "Couldn't convert var %s to string!", var->name);
            g_set_error(error, DBUS_GERROR, DBUS_GERROR_FAILED, "Couldn't convert var %s to string!", var->name);
            for (j = 0; j < i; ++j) {
                free(var_strings[j]);
            }
            free(var_strings);
            return FALSE;
        }
        ++i;
    }

    //Null-terminate the string array
    var_strings[num_vars] = NULL;
    *OUT_vars = var_strings;

    return TRUE;
}


/* The following methods are for the UIVM battery "applet" */

gboolean xcpmd_batteries_present(XcpmdObject *this, GArray* *OUT_batteries, GError **error)
{
    unsigned int i;
    GArray * batteries;

    batteries = g_array_new(true, false, sizeof(int));

    for (i=0; i < num_battery_structs_allocd; ++i) {
        if (last_status[i].present == YES) {
            g_array_append_val(batteries, i);
        }
    }

    *OUT_batteries = batteries;
    return TRUE;
}


gboolean xcpmd_battery_time_to_empty(XcpmdObject *this, guint IN_bat_n, guint *OUT_time_to_empty, GError **error)
{
    int juice_left;
    int hourly_discharge_rate;

    if (IN_bat_n >= num_battery_structs_allocd) {
        g_set_error(error, DBUS_GERROR, DBUS_GERROR_FAILED, "No such battery slot: %d", IN_bat_n);
        return FALSE;
    }

    /* If the battery is not present, return 0 */
    if (last_status[IN_bat_n].present != YES) {
        *OUT_time_to_empty = 0;
        return TRUE;
    }

    /* If the battery is not currently discharging, return 0 */
    if (!(last_status[IN_bat_n].state & 0x1)) {
        *OUT_time_to_empty = 0;
        return TRUE;
    }

    juice_left = last_status[IN_bat_n].remaining_capacity;
    hourly_discharge_rate = last_status[IN_bat_n].present_rate;

    /* Let's not divide by 0 */
    if (hourly_discharge_rate == 0) {
        *OUT_time_to_empty = 0;
        return TRUE;
    }

    *OUT_time_to_empty = juice_left * 3600 / hourly_discharge_rate;

    return TRUE;
}

gboolean xcpmd_battery_time_to_full(XcpmdObject *this, guint IN_bat_n, guint *OUT_time_to_full, GError **error)
{
    int juice_left;
    int hourly_charge_rate;
    int juice_when_full;

    if (IN_bat_n >= num_battery_structs_allocd) {
        g_set_error(error, DBUS_GERROR, DBUS_GERROR_FAILED, "No such battery slot: %d", IN_bat_n);
        return FALSE;
    }

    /* If the battery is not present, return 0 */
    if (last_status[IN_bat_n].present != YES) {
        *OUT_time_to_full = 0;
        return TRUE;
    }

    /* If the battery is not currently charging, return 0 */
    if (!(last_status[IN_bat_n].state & 0x2)) {
        *OUT_time_to_full = 0;
        return TRUE;
    }

    juice_left = last_status[IN_bat_n].remaining_capacity;
    hourly_charge_rate = last_status[IN_bat_n].present_rate;
    juice_when_full = last_info[IN_bat_n].last_full_capacity;

    /* If there's no last_full_capacity, try design_capacity */
    if (juice_when_full == 0)
        juice_when_full = last_info[IN_bat_n].design_capacity;

    /* Let's not divide by 0 */
    if (hourly_charge_rate == 0) {
        *OUT_time_to_full = 0;
        return TRUE;
    }

    *OUT_time_to_full = (juice_when_full - juice_left) * 3600 / hourly_charge_rate;

    return TRUE;
}

gboolean xcpmd_battery_percentage(XcpmdObject *this, guint IN_bat_n, guint *OUT_percentage, GError **error)
{
    int juice_left;
    int juice_when_full;

    if (battery_slot_exists(IN_bat_n) != YES) {
        g_set_error(error, DBUS_GERROR, DBUS_GERROR_FAILED, "No such battery slot: %d", IN_bat_n);
        return FALSE;
    }

    /* If the battery is not present, fail */
    if (last_status[IN_bat_n].present != YES) {
        g_set_error(error, DBUS_GERROR, DBUS_GERROR_FAILED, "No battery in slot: %d", IN_bat_n);
        return FALSE;
    }

    juice_left = last_status[IN_bat_n].remaining_capacity;
    juice_when_full = last_info[IN_bat_n].last_full_capacity;

    /* If there's no last_full_capacity, try design_capacity */
    if (juice_when_full == 0)
        juice_when_full = last_info[IN_bat_n].design_capacity;

    /* Let's not divide by 0 */
    if (juice_when_full == 0) {
        g_set_error(error, DBUS_GERROR, DBUS_GERROR_FAILED, "Unhappy battery: %d", IN_bat_n);
        return FALSE;
    }

    *OUT_percentage = juice_left * 100 / juice_when_full;

    return TRUE;
}

gboolean xcpmd_battery_is_present(XcpmdObject *this, guint IN_bat_n, gboolean *OUT_is_present, GError **error)
{
    if (battery_slot_exists(IN_bat_n) != YES) {
        g_set_error(error, DBUS_GERROR, DBUS_GERROR_FAILED, "No such battery slot: %d", IN_bat_n);
        return FALSE;
    }

    if (last_status[IN_bat_n].present == YES) {
        *OUT_is_present = TRUE;
    } else {
        *OUT_is_present = FALSE;
    }

    return TRUE;
}

gboolean xcpmd_battery_state(XcpmdObject *this, guint IN_bat_n, guint *OUT_state, GError **error)
{
    /* 0: Unknown */
    /* 1: Charging */
    /* 2: Discharging */
    /* 3: Empty */
    /* 4: Fully charged */
    /* 5: Pending charge */
    /* 6: Pending discharge */

    int juice_left;
    int juice_when_full;
    int percent;
    unsigned int i;

    if (battery_slot_exists(IN_bat_n) != YES) {
        g_set_error(error, DBUS_GERROR, DBUS_GERROR_FAILED, "No such battery slot: %d", IN_bat_n);
        return FALSE;
    }

    if (last_status[IN_bat_n].state & 0x1)
        *OUT_state = 2;
    else if (last_status[IN_bat_n].state & 0x2)
        *OUT_state = 1;
    else {
        /* We're not charging nor discharging... */
        juice_left = last_status[IN_bat_n].remaining_capacity;
        juice_when_full = last_info[IN_bat_n].last_full_capacity;
        percent = juice_left * 100 / juice_when_full;
        /* Are we full or empty? */
        if (percent > 90)
            *OUT_state = 4;
        else if (percent < 10)
            *OUT_state = 3;
        else {
            /* Is anybody else (dis)charging? */
            for (i = 0; i < num_battery_structs_allocd; ++i) {
                if (i != IN_bat_n &&
                    last_status[i].present == YES &&
                    (last_status[i].state & 0x1 || last_status[i].state & 0x2)) {
                    break;
                }
            }
            if (i < num_battery_structs_allocd) {
                /* Yes! */
                /* If the other battery is charging, we're pending charge */
                if (last_status[i].state & 0x2)
                    *OUT_state = 5;
                /* If the other battery is discharging, we're pending discharge */
                else if (last_status[i].state & 0x1)
                    *OUT_state = 6;
            } else
                /* We tried everything, the state is unknown... */
                *OUT_state = 0;
        }
    }

    return TRUE;
}

/* End of UIVM battery methods */

gboolean xcpmd_get_ac_adapter_state(XcpmdObject * this, guint *ac_ret, GError **error)
{
    *ac_ret = xenstore_read_uint(XS_AC_ADAPTER_STATE_PATH);
    return TRUE;
}

gboolean xcpmd_get_current_battery_level(XcpmdObject * this, guint *battery_level, GError **error)
{
    *battery_level = xenstore_read_uint(XS_CURRENT_BATTERY_LEVEL);
    return TRUE;
}

gboolean xcpmd_get_current_temperature(XcpmdObject * this, guint *cur_temp_ret, GError **error)
{
    *cur_temp_ret = xenstore_read_uint(XS_CURRENT_TEMPERATURE);
    return TRUE;
}

gboolean xcpmd_get_critical_temperature(XcpmdObject * this, guint *crit_temp_ret, GError **error)
{
    *crit_temp_ret = xenstore_read_uint(XS_CRITICAL_TEMPERATURE);
    return TRUE;
}

gboolean xcpmd_get_bif(XcpmdObject * this, char **bif_ret, GError **error)
{
    *bif_ret = xenstore_read(XS_BIF);
    return TRUE;
}

gboolean xcpmd_get_bst(XcpmdObject * this, char **bst_ret, GError **error)
{
    *bst_ret = xenstore_read(XS_BST);
    return TRUE;
}

gboolean xcpmd_indicate_input(XcpmdObject *this, gint input_value, GError **error)
{
    return (xcpmd_process_input(input_value) == 0) ? TRUE : FALSE;
}

gboolean xcpmd_hotkey_switch(XcpmdObject *this, const gboolean reset, GError **error)
{
    /* That's not used anymore.
       TODO: remove from idl and whatever calls it */
    return TRUE;
}

xcdbus_conn_t *xcpmd_get_xcdbus_conn(void)
{
    return xcdbus_conn;
}

int xcpmd_dbus_initialize(void)
{
    GError *error = NULL;
    DBusConnection *dbus_conn;
    DBusGConnection *gdbus_conn;
    XcpmdObject *xcpmd_obj;

    g_type_init();
    gdbus_conn = dbus_g_bus_get(DBUS_BUS_SYSTEM, &error);
    if ( gdbus_conn == NULL )
    {
        xcpmd_log(LOG_ERR, "Unable to get dbus connection, error: %d - %s\n",
                  error->code, error->message);
        g_error_free(error);
        return -1;
    }

    xcdbus_conn = xcdbus_init_event(XCPMD_SERVICE, gdbus_conn);
    if ( xcdbus_conn == NULL )
    {
        xcpmd_log(LOG_ERR, "DBus server failed get XC dbus connections\n");
        return -1;
    }

    /* export server object */
    xcpmd_obj = xcpmd_export_dbus(gdbus_conn, XCPMD_PATH);
    if ( !xcpmd_obj )
    {
        xcpmd_log(LOG_ERR, "DBus server failed in export xcpmd server object\n");
        xcdbus_shutdown(xcdbus_conn);
        xcdbus_conn = NULL;
        return -1;
    }

    xcpmd_log(LOG_INFO, "DBus server initialized.\n");

    return 0;
}

void xcpmd_dbus_cleanup(void)
{
    xcpmd_log(LOG_INFO, "DBus server cleanup\n");

    if ( xcdbus_conn != NULL )
        xcdbus_shutdown(xcdbus_conn);

    xcdbus_conn = NULL;
}
