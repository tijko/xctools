/*
 * bcl.c
 *
 * PM util brightness control routines.
 *
 * Copyright (c) 2011 Ross Philipson <ross.philipson@citrix.com>
 * Copyright (c) 2011  Citrix Systems, Inc.
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
#include <sys/ioctl.h>
#include "pmutil.h"
#include "rpcgen/surfman_client.h"

void bcl_adjust_brightness(int increase)
{
    DBusGConnection *gdbus_conn = NULL;
    GError *gerror = NULL;

    g_type_init();

    gdbus_conn = dbus_g_bus_get(DBUS_BUS_SYSTEM, &gerror);
    if ( gdbus_conn == NULL )
    {
        printf("Unable to get dbus connection, error: %d - %s\n", gerror->code, gerror->message);
        g_error_free(gerror);
        return;
    }

    if ( increase )
    {
        com_citrix_xenclient_surfman_increase_brightness_(gdbus_conn, SURFMAN_SERVICE, SURFMAN_PATH);
        printf("Backlight brightness increased.\n");
    }
    else
    {
        com_citrix_xenclient_surfman_decrease_brightness_(gdbus_conn, SURFMAN_SERVICE, SURFMAN_PATH);
        printf("Backlight brightness decreased.\n");
    }

    dbus_g_connection_unref(gdbus_conn);
}

void bcl_control(int disable)
{
    int ret, err;

    ret = xenacpi_vid_brightness_switch(disable, &err);    
    if ( disable )
    {    
        if ( ret != -1 )
            printf("ACPI _BCL disabled\n");
        else
            fprintf(stderr, "ACPI _BCL disable failed - %d\n", err);
    }
    else
    {
        if ( ret != -1 )
            printf("ACPI _BCL enabled\n");
        else
            fprintf(stderr, "ACPI _BCL enable failed - %d\n", err);
    }
}

void bcl_list_levels(void)
{
    static struct xenacpi_vid_brightness_levels *brightness_levels;
    int ret, err;
    uint32_t i;

    ret = xenacpi_vid_brightness_levels(&brightness_levels, &err);
    if ( ret == -1 )
    {
        fprintf(stderr, "Failed to get brightness levels from the firmware - error: %d\n", err);
        return;
    }

    printf("ACPI Brightness Levels Listing\n");
    printf("-------------------------------------\n");    
    printf("Brightness levels:");
    for (i = 0; i < brightness_levels->level_count; i++)
        printf(" %d", brightness_levels->levels[i]);
    printf("\n");
    printf("      Level count: %d\n", (int)brightness_levels->level_count);
    printf("      First level: %d  - brightness on full (AC) power\n",
           brightness_levels->levels[0]);
    printf("     Second level: %d  - brightness on half (50%) power\n",
           brightness_levels->levels[1]);
    printf("Remainder is a listing of the levels in ascending order\n");

    xenacpi_free_buffer(brightness_levels);
}
