/*
 * acpi-events.h
 *
 * Various ACPI values from kernel code
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

#ifndef ACPI_EVENTS_H_
#define ACPI_EVENTS_H_

// From drivers/acpi/ac.c and sbs.c
#define ACPI_AC_CLASS                   "ac_adapter"
#define ACPI_AC_DIR_NAME                "AC0"
#define ACPI_AC_FILE_STATE              "state"
#define ACPI_AC_NOTIFY_STATUS           0x80
#define ACPI_AC_STATUS_OFFLINE          0x00
#define ACPI_AC_STATUS_ONLINE           0x01
#define ACPI_AC_STATUS_UNKNOWN          0xFF

// From drivers/acpi/battery.h, battery.c, and sbs.c
#define ACPI_BATTERY_CLASS              "battery"
#define ACPI_BATTERY_DIR_NAME           "BAT%i"
#define ACPI_BATTERY_NOTIFY_STATUS      0x80
#define ACPI_BATTERY_NOTIFY_INFO        0x81
#define ACPI_BATTERY_NOTIFY_THRESHOLD   0x82
#define ACPI_BATTERY_STATE_DISCHARGING  0x1
#define ACPI_BATTERY_STATE_CHARGING     0x2
#define ACPI_BATTERY_STATE_CRITICAL     0x4
#define ACPI_BATTERY_VALUE_UNKNOWN      0xFFFFFFFF

// From drivers/acpi/processor_thermal.c, processor_driver.c, and processor_perflib.c
#define ACPI_PROCESSOR_CLASS                "processor"
#define ACPI_PROCESSOR_NOTIFY_PERFORMANCE   0x80
#define ACPI_PROCESSOR_NOTIFY_POWER         0x81
#define ACPI_PROCESSOR_NOTIFY_THROTTLING    0x82
#define ACPI_PROCESSOR_FILE_PERFORMANCE     "performance"
#define ACPI_PROCESSOR_NOTIFY_PERFORMANCE   0x80

// From drivers/acpi/acpi_pad.c
#define ACPI_PROCESSOR_AGGREGATOR_CLASS     "acpi_pad"
#define ACPI_PROCESSOR_AGGREGATOR_NOTIFY    0x80

// From drivers/acpi/ec.c
#define ACPI_EC_CLASS                   "embedded_controller"

// From drivers/acpi/thermal.c
#define ACPI_THERMAL_CLASS              "thermal_zone"
#define ACPI_THERMAL_NOTIFY_TEMPERATURE 0x80
#define ACPI_THERMAL_NOTIFY_THRESHOLDS  0x81
#define ACPI_THERMAL_NOTIFY_DEVICES     0x82
#define ACPI_THERMAL_NOTIFY_CRITICAL    0xF0
#define ACPI_THERMAL_NOTIFY_HOT         0xF1
#define ACPI_THERMAL_MODE_ACTIVE        0x00
#define ACPI_THERMAL_MAX_ACTIVE         10
#define ACPI_THERMAL_MAX_LIMIT_STR_LEN  65

// From drivers/acpi/button.c
#define ACPI_BUTTON_CLASS               "button"
#define ACPI_BUTTON_SUBCLASS_POWER      "power"
#define ACPI_BUTTON_SUBCLASS_SLEEP      "sleep"
#define ACPI_BUTTON_SUBCLASS_LID        "lid"
#define ACPI_BUTTON_FILE_INFO           "info"
#define ACPI_BUTTON_FILE_STATE          "state"
#define ACPI_BUTTON_TYPE_UNKNOWN        0x00
#define ACPI_BUTTON_NOTIFY_STATUS       0x80
#define ACPI_BUTTON_HID_POWER           "PNP0C0C"
#define ACPI_BUTTON_HID_SLEEP           "PNP0C0E"
#define ACPI_BUTTON_HID_LID             "PNP0C0D"
#define ACPI_BUTTON_TYPE_POWER          0x01
#define ACPI_BUTTON_TYPE_SLEEP          0x03
#define ACPI_BUTTON_TYPE_LID            0x05

//From drivers/acpi/pci_link.c
#define ACPI_PCI_LINK_CLASS             "pci_irq_routing"
#define ACPI_PCI_LINK_FILE_INFO         "info"
#define ACPI_PCI_LINK_FILE_STATUS       "state"
#define ACPI_PCI_LINK_MAX_POSSIBLE      16

// From drivers/acpi/pci_root.c
#define ACPI_PCI_ROOT_CLASS             "pci_bridge"

// From drivers/acpi/acpi_memhotplug.c
#define ACPI_MEMORY_DEVICE_CLASS        "memory"
#define ACPI_MEMORY_DEVICE_HID          "PNP0C80"

// From drivers/acpi/sbshc.c
#define ACPI_SMB_HC_CLASS               "smbus_host_ctl"

// From drivers/acpi/sbs.c
#define ACPI_SBS_CLASS                  "sbs"
#define ACPI_SBS_FILE_INFO              "info"
#define ACPI_SBS_FILE_STATE             "state"
#define ACPI_SBS_FILE_ALARM             "alarm"
#define ACPI_SBS_NOTIFY_STATUS          0x80
#define ACPI_SBS_NOTIFY_INFO            0x81
#define ACPI_SBS_BLOCK_MAX              32

// From drivers/acpi/scan.c
#define ACPI_BUS_CLASS                   "system_bus"
#define ACPI_BUS_HID                    "LNXSYBUS"

// From drivers/acpi/power.c
#define ACPI_POWER_CLASS                    "power_resource"
#define ACPI_POWER_FILE_INFO                "info"
#define ACPI_POWER_FILE_STATUS              "state"
#define ACPI_POWER_RESOURCE_STATE_OFF       0x00
#define ACPI_POWER_RESOURCE_STATE_ON        0x01
#define ACPI_POWER_RESOURCE_STATE_UNKNOWN   0xFF

// From acpid2/input_layer.c
#define ACPI_BUTTON_SUBCLASS_SUSPEND    "suspend"
#define ACPI_VIDEO_CLASS                "video"
#define ACPI_VIDEO_SUBCLASS_BRTUP       "brightnessup"
#define ACPI_VIDEO_SUBCLASS_BRTDN       "brightnessdown"
#define ACPI_VIDEO_SUBCLASS_BRTCYCLE    "brightnesscycle"
#define ACPI_VIDEO_SUBCLASS_TABLETMODE  "tabletmode"

#endif
