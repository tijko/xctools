/*
 * netlink.h
 *
 * Netlink ACPI definitions from kernel code
 *
 * Copyright (c) 2012 Aurelien Chartier <aurelien.chartier@citrix.com>
 * Copyright (c) 2012 Citrix Systems, Inc.
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

#ifndef NETLINK_H_
#define NETLINK_H_

/* From drivers/acpi/event.c in the kernel code */

struct acpi_genl_event
{
        char device_class[20];
        char bus_id[15];
        uint32_t type;
        uint32_t data;
};

enum {
        ACPI_GENL_ATTR_UNSPEC,
        ACPI_GENL_ATTR_EVENT,   /* ACPI event info needed by user space */
        __ACPI_GENL_ATTR_MAX,
};
#define ACPI_GENL_ATTR_MAX (__ACPI_GENL_ATTR_MAX - 1)

#define ACPI_EVENT_FAMILY_NAME		"acpi_event"
#define ACPI_EVENT_MCAST_GROUP_NAME	"acpi_mc_group"

#endif /* NETLINK_H_ */
