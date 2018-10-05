/*
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

#ifndef __VM_EVENTS_MODULE_H__
#define __VM_EVENTS_MODULE_H__

#define EVENT_VM_CREATING   0
#define EVENT_VM_STOPPING   1
#define EVENT_VM_REBOOTING  2
#define EVENT_VM_RUNNING    3
#define EVENT_VM_STOPPED    4
#define EVENT_VM_PAUSED     5

//Names required for dynamic loading
#define VM_EVENTS_MODULE_SONAME "vm-events-module.so"
#define VM_EVENTS               "_vm_event_table"

#endif
