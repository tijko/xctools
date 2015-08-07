/*
 * xcpmd.c
 *
 * XenClient platform management daemon
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

static char rcsid[] = "$Id:$";

/*
 * $Log:$
 */

/* Xen extended power management support provides HVM guest power management
 * features beyond S3, S4, S5.  For example, it helps expose system level
 * battery status and battery meter information and in future will be extended
 * to include more power management support.  This extended power management
 * support is enabled by setting xen_extended_power_mgmt to 1 or 2 in the HVM
 * config file.  When set to 2, non-pass through mode is enabled which heavily
 * relies on this power management daemon to glean battery information from
 * dom0 and store it xenstore which would then be queries and used by qemu and
 * passed to the guest when appropriate battery ports are read/written to.
 */

#include "project.h"
#include "xcpmd.h"


int main(int argc, char *argv[]) {

    int ret = 0;

#ifndef RUN_STANDALONE
    openlog("xcpmd", 0, LOG_DAEMON);
    daemonize();
#endif

    xcpmd_log(LOG_INFO, "Starting XenClient power management daemon.\n");

    //Initialize libevent library
    event_init();

    //Initialize xenstore.
    if (xenstore_init() == -1) {
        xcpmd_log(LOG_ERR, "Unable to init xenstore\n");
        return -1;
    }

    // Allow everyone to read from /pm/ in xenstore
    xenstore_rm("/pm");
    xenstore_mkdir("/pm");
    xenstore_chmod("r0", 1, "/pm");

    initialize_platform_info();

    // Initialize xcpmd services
    if (xcpmd_dbus_initialize() == -1) {
        xcpmd_log(LOG_ERR, "Failed to initialize DBUS server\n");
        goto xcpmd_err;
    }

    if (acpi_events_initialize() == -1) {
        xcpmd_log(LOG_ERR, "Failed to initialize ACPI events monitor\n");
        goto xcpmd_err;
    }

    // Run main server loop
    event_dispatch();

    goto xcpmd_out;

xcpmd_err:
    ret = -1;
xcpmd_out:
    acpi_events_cleanup();
    xcpmd_dbus_cleanup();
#ifndef RUN_STANDALONE
    closelog();
#endif

    return ret;
}
