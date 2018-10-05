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

#ifndef __ACPI_MODULE_H__
#define __ACPI_MODULE_H__

#define EVENT_PWR_BTN       0
#define EVENT_SLP_BTN       1
#define EVENT_SUSP_BTN      2
#define EVENT_BCL           3
#define EVENT_LID           4
#define EVENT_ON_AC         5
#define EVENT_TABLET_MODE   6
#define EVENT_BATT_STATUS   7
#define EVENT_BATT_INFO     8

//Names required for dynamic loading
#define ACPI_MODULE_SONAME          "acpi-module.so"
#define ACPI_EVENTS                 "_acpi_event_table"

#endif
