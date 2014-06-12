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
    /* Set a flag and synchronize the switch work with the main select loop. */
    hp_hotkey_cmd = (reset ? HP_HOTKEY_RESET : HP_HOTKEY_SET);
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
