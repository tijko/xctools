/*
 * Copyright (c) 2011 Citrix Systems, Inc.
 * Copyright (c) 2015 Assured Information Security, Inc.
 *
 * Authors:
 * Kamala Narasimhan <kamala.narasimhan@citrix.com>
 * Ross Philipson <philipsonr@ainfosec.com>
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

#ifndef __BATTERY_H__
#define __BATTERY_H__

#include "project.h"
#include "xcpmd.h"


//Battery info for consumption by dbus and others
extern struct battery_info   *last_info;
extern struct battery_status *last_status;
extern unsigned int num_battery_structs_allocd;

extern struct event refresh_battery_event;

int get_battery_percentage(unsigned int battery_index);
int get_num_batteries_present(void);
int get_num_batteries(void);
void update_batteries(void);
int update_battery_status(unsigned int battery_index);
int update_battery_info(unsigned int battery_index);
void write_battery_status_to_xenstore(unsigned int battery_index);
void write_battery_info_to_xenstore(unsigned int battery_index);
int get_overall_battery_percentage(void);
int get_current_battery_level(void);
void wrapper_refresh_battery_event(int fd, short event, void *opaque);
int battery_slot_exists(unsigned int battery_index);
int battery_is_present(unsigned int battery_index);


#endif
