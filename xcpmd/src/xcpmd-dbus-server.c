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

xcdbus_conn_t *xcdbus_conn = NULL;

extern struct battery_info   last_info[MAX_BATTERY_SUPPORTED];
extern struct battery_status last_status[MAX_BATTERY_SUPPORTED];

/* The following methods are for the UIVM battery "applet" */

gboolean xcpmd_batteries_present(XcpmdObject *this, GArray* *OUT_batteries, GError **error)
{
    int i;
    GArray * batteries;
    
    batteries = g_array_new(true, false, sizeof(int));

    for (i=0; i < MAX_BATTERY_SUPPORTED; ++i) {
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

    if (IN_bat_n >= MAX_BATTERY_SUPPORTED) {
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

    if (IN_bat_n >= MAX_BATTERY_SUPPORTED) {
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

    if (IN_bat_n >= MAX_BATTERY_SUPPORTED) {
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
    if (IN_bat_n >= MAX_BATTERY_SUPPORTED) {
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

    if (IN_bat_n >= MAX_BATTERY_SUPPORTED) {
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
            for (i = 0; i < MAX_BATTERY_SUPPORTED; ++i) {
                if (i != IN_bat_n &&
                    last_status[i].present == YES &&
                    (last_status[i].state & 0x1 || last_status[i].state & 0x2)) {
                    break;
                }
            }
            if (i < MAX_BATTERY_SUPPORTED) {
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
