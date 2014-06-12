/*
 * platform.c
 *
 * XenClient platform management daemon platform quirks/specs setup
 *
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

#include <sys/mman.h>
#include <ctype.h>
#include <fcntl.h>
#include <pci/header.h>
#include <pci/pci.h>
#include "project.h"
#include "xcpmd.h"

/* Manufacturers */
#define MANUFACTURER_HP          "hewlett-packard"
#define MANUFACTURER_DELL        "dell"
#define MANUFACTURER_TOSHIBA     "toshiba"
#define MANUFACTURER_PANASONIC   "panasonic"
#define MANUFACTURER_LENOVO      "lenovo"
#define MANUFACTURER_FUJITSU     "fujitsu"
#define MANUFACTURER_FUJTSU      "fujtsu"
#define MANUFACTURER_APPLE      "Apple Inc."

/* Dell 5X20 series */
#define DELL_E5220               "E5220"
#define DELL_E5420               "E5420"
#define DELL_E5520               "E5520"
#define TOSHIBA_TECRA            "TECRA"

/* PCI Values */
#define PCI_VENDOR_DEVICE_OFFSET 0x0
#define PCI_CLASS_REV_OFFSET     0x8
#define PCI_VIDEO_VGA_CLASS_ID   0x0300

/* PCI Intel Values */
#define INTEL_VENDOR_ID          0x8086
#define MONTEVINA_GMCH_ID        0x2a40
#define CALPELLA_GMCH_ID         0x0044
#define SANDYBRIDGE_GMCH_ID      0x0104

#define PCI_VENDOR_ID_WORD(v) ((uint16_t)(0xffff & (v)))
#define PCI_DEVICE_ID_WORD(v) ((uint16_t)(0xffff & (v >> 16)))
#define PCI_CLASS_ID_WORD(v) ((uint16_t)(0xffff & (v >> 16)))

#define SCAN_ROM_BIOS_BASE 0xF0000
#define SCAN_ROM_BIOS_SIZE 0x10000

uint32_t pm_quirks = PM_QUIRK_NONE;
uint32_t pm_specs = PM_SPEC_NONE;

/* SMBIOS Lengths */
#define SMBIOS_SM_LENGTH       0x20
#define SMBIOS_DMI_LENGTH      0x0F
#define SMBIOS_HEADER_LENGTH   0x04

/* SMBIOS Offsets */
#define SMBIOS_EPS_STRING      0x00 /* 4 BYTES "_SM_" anchor string */
#define SMBIOS_EPS_CHECKSUM    0x04 /* BYTE CS sums to zero when added to bytes in EPS */
#define SMBIOS_EPS_LENGTH      0x05 /* BYTE Length of the Entry Point Structure */
#define SMBIOS_MAJOR_VERSION   0x06 /* BYTE */
#define SMBIOS_MINOR_VERSION   0x07 /* BYTE */
#define SMBIOS_MAX_STRUCT_SIZE 0x08 /* WORD Size of the largest SMBIOS structure */
#define SMBIOS_REVISION        0x0A /* BYTE */
#define SMBIOS_FORMATTED_AREA  0x0B /* 5 BYTES, see spec for revision */
#define SMBIOS_IEPS_STRING     0x10 /* 5 BYTES "_DMI_" intermediate anchor string */
#define SMBIOS_IEPS_CHECKSUM   0x15 /* BYTE CS sums to zero when added to bytes in IEPS */
#define SMBIOS_TABLE_LENGTH    0x16 /* WORD Total length of SMBIOS Structure Table */
#define SMBIOS_TABLE_ADDRESS   0x18 /* DWORD The 32-bit physical starting address of the read-only SMBIOS Structures */
#define SMBIOS_STRUCT_COUNT    0x1C /* WORD Total number of structures present in the SMBIOS Structure Table */
#define SMBIOS_BCD_REVISION    0x1E /* BYTE */

#define SMBIOS_STRUCT_TYPE     0x00 /* BYTE Specifies the type of structure */
#define SMBIOS_STRUCT_LENGTH   0x01 /* BYTE Specifies the length of the formatted area of the structure */
#define SMBIOS_STRUCT_HANDLE   0x02 /* WORD Specifies 16-bit number in the range 0 to 0FFFEh */

/* SMBIOS Types */
#define SMBIOS_TYPE_BIOS_INF0    0
#define SMBIOS_TYPE_SYSTEM_INFO  1
#define SMBIOS_TYPE_BASE_BOARD   2
#define SMBIOS_TYPE_ENCLOSURE    3
#define SMBIOS_TYPE_INACTIVE     126
#define SMBIOS_TYPE_EOT          127
#define SMBIOS_TYPE_VENDOR_MIN   128
#define SMBIOS_TYPE_VENDOR_MAX   255

/* Xenstore permissions */
#define XENSTORE_READ_ONLY      "r0"

struct smbios_locator {
    size_t phys_addr;
    uint16_t length;
    uint16_t count;
    uint8_t *addr;
};

struct smbios_header {
	uint8_t type;
	uint8_t length;
	uint16_t handle;
} __attribute__ ((packed));

struct smbios_bios_info {
	struct smbios_header header;
	uint8_t vendor_str;
	uint8_t version_str;
	uint16_t starting_address_segment;
	uint8_t release_date_str;
	uint8_t rom_size;
	uint8_t characteristics[8];
	uint8_t characteristics_extension_bytes[2];
	uint8_t major_release;
	uint8_t minor_release;
	uint8_t embedded_controller_major;
	uint8_t embedded_controller_minor;
} __attribute__ ((packed));

struct smbios_system_info {
	struct smbios_header header;
	uint8_t manufacturer_str;
	uint8_t product_name_str;
	uint8_t version_str;
	uint8_t serial_number_str;
	uint8_t uuid[16];
	uint8_t wake_up_type;
	uint8_t sku_str;
	uint8_t family_str;
} __attribute__ ((packed));

static int smbios_entry_point(struct smbios_locator *locator, uint8_t *entry_point, int is_eps)
{
    uint8_t cs;
    uint32_t count;

    if (is_eps)
    {
        /* checksum sanity check on _SM_ entry point */
        for ( cs = 0, count = 0; count < entry_point[SMBIOS_EPS_LENGTH]; count++ )
            cs += entry_point[count];
        if ( cs != 0 )
        {
            xcpmd_log(LOG_WARNING, "Invalid _SM_ checksum\n");
            return -1;
        }
        /* nothing else really interesting in the EPS, move to the IEPS */
        entry_point += SMBIOS_IEPS_STRING;
        if ( memcmp(entry_point, "_DMI_", 5) != 0 )
        {
            xcpmd_log(LOG_WARNING, "Entry point structure missing _DMI_ anchor\n");
            return -1;
        }
    }

    /* entry point is IEPS, do checksum of this portion */
    for ( cs = 0, count = 0; count < SMBIOS_DMI_LENGTH; count++ )
        cs += entry_point[count];
    if ( cs != 0 )
    {
        xcpmd_log(LOG_WARNING, "Invalid _DMI_ checksum\n");
        return -1;
    }

    locator->phys_addr = (*(uint32_t*)(entry_point + SMBIOS_TABLE_ADDRESS - SMBIOS_IEPS_STRING));
    locator->length = (*(uint16_t*)(entry_point + SMBIOS_TABLE_LENGTH - SMBIOS_IEPS_STRING));
    locator->count = (*(uint16_t*)(entry_point + SMBIOS_STRUCT_COUNT - SMBIOS_IEPS_STRING));

    /* sanity */
    if ( (locator->length < 4) || (locator->count == 0) ) {
        xcpmd_log(LOG_WARNING, "Entry point structure reporting invalid length or count\n");
        return -1;
    }

    locator->addr = map_phys_mem(locator->phys_addr, locator->length);
    if ( locator->addr == NULL )
    {
        xcpmd_log(LOG_ERR, "Failed to map SMBIOS structures at phys="UINT_FMT"\n", locator->phys_addr);
        return -1;
    }

    return 0;
}

static int smbios_locate_structures(struct smbios_locator *locator)
{
    size_t loc = 0;
    uint8_t *addr;
    int rc = -1;

    memset(locator, 0, sizeof(struct smbios_locator));

    /* use EFI tables if present */
    rc = find_efi_entry_location("SMBIOS", 6, &loc);
    if ( (rc == 0) && (loc != 0) ) {
        addr = map_phys_mem(loc, SMBIOS_SM_LENGTH);
        if ( addr == NULL )
        {
            xcpmd_log(LOG_ERR, "Failed to map SMBIOS entry point structure at phys="UINT_FMT"\n", loc);
            return -1;
        }
        rc = smbios_entry_point(locator, addr, 1);
        unmap_phys_mem(addr, SMBIOS_SM_LENGTH);
        return rc;
    }

    /* Locate SMBIOS entry via memory scan of ROM region */
    addr = map_phys_mem(SCAN_ROM_BIOS_BASE, SCAN_ROM_BIOS_SIZE);
    if ( addr == NULL )
    {
        xcpmd_log(LOG_ERR, "Failed to map ROM BIOS at phys=%x\n", SCAN_ROM_BIOS_BASE);
        return -1;
    }

    for ( loc = 0; loc <= (SCAN_ROM_BIOS_SIZE - SMBIOS_SM_LENGTH); loc += 16 ) { /* stop before 0xFFE0 */
        /* Look for _SM_ signature for newer entry point which preceeds _DMI_, else look for only the older _DMI_ */
        if ( memcmp(addr + loc, "_SM_", 4) == 0 )
        {
            rc = smbios_entry_point(locator, addr + loc, 1);
            if ( rc == 0 ) /* found it */
                break;
        }
        else if ( memcmp(addr + loc, "_DMI_", 5) == 0 )
        {
            rc = smbios_entry_point(locator, addr + loc, 0);
            if ( rc == 0 ) /* found it */
                break;
        }
    }
    unmap_phys_mem(addr, SCAN_ROM_BIOS_SIZE);

    return rc;
}

static void *smbios_locate_structure_instance(struct smbios_locator *locator,
                                              uint8_t type, uint32_t instance,
                                              uint32_t *length_out)
{
    uint16_t idx;
    uint8_t *ptr = locator->addr;
    uint8_t *tail;
    uint32_t counter = 0;
    void *table = NULL;

    for ( idx = 0; idx < locator->count; idx++ )
    {
        if ( (ptr[SMBIOS_STRUCT_LENGTH] < 4)||
           ( (ptr + ptr[SMBIOS_STRUCT_LENGTH]) > (ptr + locator->length)) )
        {
            xcpmd_log(LOG_ERR, "Invalid SMBIOS table data detected\n");
            return NULL;
        }

        /* Run the tail pointer past the end of this struct and all strings */
        tail = ptr + ptr[SMBIOS_STRUCT_LENGTH];
        while ( (tail - ptr + 1) < locator->length )
        {
            if ( (tail[0] == 0) && (tail[1] == 0) )
                break;
            tail++;
        }
        tail += 2;

        if ( (ptr[SMBIOS_STRUCT_TYPE] == type) && (++counter == instance) )
        {
            table = ptr;
            if ( length_out != NULL )
                *length_out = (tail - ptr);

            break;
        }

        /* test for terminating structure */
        if ( ptr[SMBIOS_STRUCT_TYPE] == SMBIOS_TYPE_EOT )
        {
            /* table is done - sanity check */
            if ( idx != locator->count - 1 )
            {
                xcpmd_log(LOG_ERR, "SMBIOS missing EOT at end\n");
                return NULL;
            }
        }

        ptr = tail;
    }

    return table;
}

static void setup_wmi_ssdt_external_file(void)
{
    uint8_t *wmi_ssdt;
    uint32_t length = 0;
    FILE *fs = NULL;
    int written, err;
    const char *fname = SSDT_WMI_EXTERNAL_PATH "/" SSDT_WMI_EXTERNAL_FILE;

    wmi_ssdt = create_wmi_ssdt(&length, &err);
    if ( wmi_ssdt == NULL )
    {
        if ( err != 0 )
            xcpmd_log(LOG_ERR, "%s failed to create WMI SSDT, err out: %d\n",
                      __FUNCTION__, err);
        return;
    }

    if ( mkdir(SSDT_WMI_EXTERNAL_PATH, 0766) == -1 )
    {
        if ( errno != EEXIST )
        {
            xcpmd_log(LOG_ERR, "%s failed to create xcpmd directory - %s, %s\n",
                      __FUNCTION__, SSDT_WMI_EXTERNAL_PATH, strerror(errno));
            goto out;
        }
    }

    unlink(fname);

    fs = fopen(fname, "w");
    if ( fs == NULL )
    {
        xcpmd_log(LOG_ERR, "%s failed to open WMI SSDT file - %s\n",
                  __FUNCTION__, fname);
        goto out;
    }

    written = fwrite(wmi_ssdt, length, 1, fs);
    if ( written < 1 )
    {
        xcpmd_log(LOG_ERR, "%s failed to write WMI SSDT file - %s, error: %d\n",
                  __FUNCTION__, fname, errno);
        goto out;
    }

    if (!xenstore_write(fname, XENACPI_XS_OEM_WMI_SSDT_PATH))
    {
        xcpmd_log(LOG_ERR, "%s failed to write WMI SSDT path to xenstore: %s\n",
                  __FUNCTION__, XENACPI_XS_OEM_WMI_SSDT_PATH);
        goto out;
    }

    if (!xenstore_chmod(XENSTORE_READ_ONLY, 1, XENACPI_XS_OEM_WMI_SSDT_PATH))
    {
        xcpmd_log(LOG_ERR, "%s failed to set xenstore RO perms on: %s\n",
                  __FUNCTION__, fname);
        goto out;
    }

    xcpmd_log(LOG_INFO, "Wrote WMI SSDT file: %s\n", fname);

out:
    if (fs != NULL)
        fclose(fs);
    xenacpi_free_buffer(wmi_ssdt);
}

#define WMI_MAX_PLATFORM_DEVICES 32 /* well that certainly should be enough */
static struct wmi_platform_device wmi_pd[WMI_MAX_PLATFORM_DEVICES];

static void make_xenstore_wmi_notify_path(void)
{
    if (!xenstore_write("0", XENACPI_XS_OEM_WMI_NOTIFY_PATH))
    {
        xcpmd_log(LOG_ERR, "%s failed to write WMI notify path to xenstore: %s\n",
                  __FUNCTION__, XENACPI_XS_OEM_WMI_NOTIFY_PATH);
        return;
    }

    if (!xenstore_chmod(XENSTORE_READ_ONLY, 1, XENACPI_XS_OEM_WMI_NOTIFY_PATH))
    {
        xcpmd_log(LOG_ERR, "%s failed to set xenstore RO perms on: %s\n",
                  __FUNCTION__, XENACPI_XS_OEM_WMI_NOTIFY_PATH);
        return;
    }

    xcpmd_log(LOG_INFO, "Set WMI notify path in xenstore: %s\n", XENACPI_XS_OEM_WMI_NOTIFY_PATH);
}

static void setup_wmi_default_devices(void)
{
    /* Load a set of devices from known ACPI BIOSes as a fallback */
    strcpy(wmi_pd[0].name, "WMID");
    wmi_pd[0].wmiid = 1;
    strcpy(wmi_pd[1].name, "AMW0");
    wmi_pd[1].wmiid = 1;
    strcpy(wmi_pd[2].name, "WMI1");
    wmi_pd[2].wmiid = 1;
    strcpy(wmi_pd[3].name, "WMI2");
    wmi_pd[3].wmiid = 1;
}

static void setup_wmi_platform_devices(void)
{
    struct xenacpi_wmi_device *wdevices = NULL;
    uint32_t count, i;
    int ret, err;
    char oemwmi_path[XS_FORMAT_PATH_LEN + 1];

    make_xenstore_wmi_notify_path();

    /* Get a listing of actual WMI devices on the platform */
    ret = xenacpi_wmi_get_devices(&wdevices, &count, &err);
    if ( ret == -1 )
    {
        xcpmd_log(LOG_WARNING, "%s failed to get WMI device listing, error: %d\n", __FUNCTION__, err);
        setup_wmi_default_devices();
        return;
    }

    if ( count == 0 )
    {
        xcpmd_log(LOG_WARNING, "%s no WMI devices reported\n", __FUNCTION__);
        setup_wmi_default_devices();
        goto out;
    }

    for ( i = 0; i < count; i++ )
    {
        if (i >= WMI_MAX_PLATFORM_DEVICES)
        {
            xcpmd_log(LOG_WARNING, "%s exceeded max WMI devices: %d\n", __FUNCTION__, (int)i);
            break;
        }

        xenacpi_wmi_extract_name(wmi_pd[i].name, wdevices[i].name);
        wmi_pd[i].wmiid = wdevices[i].wmiid;
        sprintf(wmi_pd[i].bus_name, "%s:%02x", wdevices[i]._hid, atoi(wdevices[i]._uid));
#ifdef XCPMD_DEBUG
        xcpmd_log(LOG_DEBUG, "~WMI device: %s WMIID: %d, BUS %s\n",
                  wmi_pd[i].name, (int)wmi_pd[i].wmiid, wmi_pd[i].bus_name);
#endif
    }

    xcpmd_log(LOG_INFO, "Loaded %d platform WMI devices.\n", (int)count);

out:
    xenacpi_free_buffer(wdevices);
}

#define WMI_HP_GUID_WMAA "\x34\xF0\xB7\x5F\x63\x2C\xE9\x45\xBE\x91\x3D\x44\xE2\xC7\x07\xE4"

#define WMI_HP_SIGNATURE         0x55434553 /* SECU */
#define WMI_HP_CMD_SETBIOS       0x00000002 /* Set BIOS */
#define WMI_HP_CMDT_SETHOTKEY    0x00000009 /* Set BIOS SHK hotkey mapping */
#define WMI_HP_DATA_SIZE         0x00000004
#define WMI_HP_DATA_HOTKEY_SET   0x0000006E
#define WMI_HP_DATA_HOTKEY_RESET 0x00000000
#define WMI_HP_SIGRETURN_PASS    0x53534150 /* PASS */
#define WMI_HP_SIGRETURN_FAIL    0x4C494146 /* FAIL */

struct wmi_bios_args {
    uint32_t signature;
    uint32_t command;
    uint32_t command_type;
    uint32_t data_size;
    uint32_t data;
} __attribute__ ((packed));

struct wmi_bios_return {
    uint32_t sigpass;
    uint32_t return_code;
} __attribute__ ((packed));

static struct xenacpi_wmi_invocation_data invocation_data;
static struct wmi_bios_args hp_command_hotkeys_arg;

enum HP_HOTKEY_CMD hp_hotkey_cmd = HP_HOTKEY_NONE;

static void setup_hp_wmi_hotkeys_data_blocks(void)
{
    if ( (pm_quirks & PM_QUIRK_HP_HOTKEY_SWITCH) == 0 )
        return; /* currently there is only an HP use of this */

    memcpy(&invocation_data.guid[0], WMI_HP_GUID_WMAA, XENACPI_WMI_GUID_SIZE);
    invocation_data.objid[0] = 'A';
    invocation_data.objid[1] = 'A';
    invocation_data.flags = XENACPI_WMI_FLAG_USE_OBJID;
    invocation_data.instance = 0;
    invocation_data.method_id = 1;

    hp_command_hotkeys_arg.signature = WMI_HP_SIGNATURE;
    hp_command_hotkeys_arg.command = WMI_HP_CMD_SETBIOS;
    hp_command_hotkeys_arg.command_type = WMI_HP_CMDT_SETHOTKEY;
    hp_command_hotkeys_arg.data_size = WMI_HP_DATA_SIZE;
}

static void setup_xenstore_platform_bcl_count(void)
{
    struct xenacpi_vid_brightness_levels *levels;
    int ret, err, len;
    char count_str[XS_FORMAT_PATH_LEN + 1];

    ret = xenacpi_vid_brightness_levels(&levels, &err);
    if ( ret == -1 )
    {
        xcpmd_log(LOG_ERR, "Failed to get BCL information from the platform firmware - %d\n", err);
        return;
    }

    if ( levels->level_count < 4 )
    {
        xcpmd_log(LOG_ERR, "Invalid BCL information for the platform.\n");
        goto out;
    }

    len = snprintf(count_str, XS_FORMAT_PATH_LEN, "%d", (int)levels->level_count);
    if (!xenstore_write(count_str, "/pm/bcl_count"))
    {
        xcpmd_log(LOG_ERR, "Failed to write BCL count to xenstore /pm/bcl_count\n");
        goto out;
    }

    if (!xenstore_chmod(XENSTORE_READ_ONLY, 1, "/pm/bcl_count"))
    {
        xcpmd_log(LOG_ERR, "Failed to set xenstore RO perms on /pm/bcl_count\n");
        goto out;
    }

    xcpmd_log(LOG_INFO, "Platform BCL count: %d written to xenstore.\n", (int)levels->level_count);
#ifdef XCPMD_DEBUG
    {
        unsigned int i;

        xcpmd_log(LOG_DEBUG, "~BCL values: ");
        for ( i = 0; i < levels->level_count; i++ )
            xcpmd_log(LOG_DEBUG, "%d ", levels->levels[i]);
    }
#endif

out:
    xenacpi_free_buffer(levels);
}

static void setup_software_bcl_and_input_quirks(void)
{
    struct smbios_locator locator;
    struct smbios_system_info *system_info;
    struct smbios_bios_info *bios_info;
    uint32_t length;
    char *manufacturer, *product, *vendor, *bios_version;
    uint32_t pci_val;
    uint16_t pci_vendor_id, pci_gmch_id;
    int rc;

    memset(&locator, 0x0, sizeof (locator));

    /* Read SMBIOS information */
    rc = smbios_locate_structures(&locator);
    if ( rc != 0 )
    {
        xcpmd_log(LOG_WARNING, "%s failed to find SMBIOS info\n", __FUNCTION__);
        goto out;
    }

    system_info =
        smbios_locate_structure_instance(&locator, SMBIOS_TYPE_SYSTEM_INFO, 1, &length);
    if ( system_info == NULL )
    {
        xcpmd_log(LOG_WARNING, "%s could not locate SMBIOS_TYPE_SYSTEM_INFO table??\n", __FUNCTION__);
        goto out;
    }

    manufacturer = (char *)system_info + system_info->header.length;
    product = manufacturer + strlen(manufacturer) + 1;

    bios_info =
        smbios_locate_structure_instance(&locator, SMBIOS_TYPE_BIOS_INF0, 1, &length);
    if ( bios_info == NULL )
    {
        xcpmd_log(LOG_WARNING, "%s could not locate SMBIOS_TYPE_BIOS_INF0 table??\n", __FUNCTION__);
        goto out;
    }

    vendor = (char *)bios_info + bios_info->header.length;
    bios_version = vendor + strlen(vendor) + 1;

    /* Read PCI information */
    pci_val = pci_host_read_dword(0, 0, 0, PCI_VENDOR_DEVICE_OFFSET);
    pci_vendor_id = PCI_VENDOR_ID_WORD(pci_val);
    pci_gmch_id = PCI_DEVICE_ID_WORD(pci_val);
    if ( pci_vendor_id != INTEL_VENDOR_ID )
    {
        xcpmd_log(LOG_WARNING, "%s unknown/unsupported chipset vendor ID: %x\n", __FUNCTION__, pci_vendor_id);
        goto out;
    }
    xcpmd_log(LOG_INFO, "Platform chipset Vendor ID: %4.4x GMCH ID: %4.4x\n", pci_vendor_id, pci_gmch_id);

    /* By default, turn on SW assistance for all systems then turn it off in cases where it is
     * known to be uneeded.
     */
    pm_quirks |= PM_QUIRK_SW_ASSIST_BCL|PM_QUIRK_SW_ASSIST_BCL_IGFX_PT;

    /* Filter out the manufacturers/products that need software assistance for BCL and other
     * platform functionality
     */
    if ( strnicmp(manufacturer, MANUFACTURER_HP, strlen(MANUFACTURER_HP)) == 0 )
    {
        /* HP platforms use keyboard input for hot-keys and guest software like QLB to drive BIOS
         * functionality for those keys via WMI. The first flags allows the backend to field a few
         * of the hotkey presses when no guests are using them by processing the keyboard (via QLB).
         * The second flag allows backend control over the BIOS hotkey mapping when a VM running QLB
         * is not in focus or shutdown.
         */
        pm_quirks |= PM_QUIRK_HP_HOTKEY_INPUT;
        pm_quirks |= PM_QUIRK_HP_HOTKEY_SWITCH;

        /* Almost all the HPs used KB input for adjusting the brightness until an HDX VM is run with
         * the QLB/HotKey software. Once this VM is running, the guest software takes over and the BIOS
         * uses the IGD OpRegion to control brightness. The Sandybridge HP systems do not seem to be
         * doing what is excpected though. When it executes the WMI commands in SMM it does not return
         * the correct values to cause the ACPI brightness control code to use the IGD OpRegion so
         * the HotKey tools do not work. It does however generate the standard brightness SCI as a
         * workaround it can be handled with our support SW. TODO this needs to be addressed later.
         */
        if ( pci_gmch_id == SANDYBRIDGE_GMCH_ID )
            pm_quirks |= PM_QUIRK_SW_ASSIST_BCL_HP_SB;
        else
            pm_quirks &= ~(PM_QUIRK_SW_ASSIST_BCL|PM_QUIRK_SW_ASSIST_BCL_IGFX_PT);
    }
    else if ( strnicmp(manufacturer, MANUFACTURER_DELL, strlen(MANUFACTURER_DELL)) == 0 )
    {
        /* MV and CP systems seem to use firmware BCL control but SB and IB do not */
        if ( (pci_gmch_id == MONTEVINA_GMCH_ID) || (pci_gmch_id == CALPELLA_GMCH_ID) )
            pm_quirks &= ~(PM_QUIRK_SW_ASSIST_BCL|PM_QUIRK_SW_ASSIST_BCL_IGFX_PT);
    }
    else if ( strnicmp(manufacturer, MANUFACTURER_LENOVO, strlen(MANUFACTURER_LENOVO)) == 0 )
    {
        /* MV systems need software assistance
         * CP systems seem to use firmware
         * SB systems need software assistance including with Intel GPU passthrough
         */
        if ( pci_gmch_id == MONTEVINA_GMCH_ID )
            pm_quirks &= ~(PM_QUIRK_SW_ASSIST_BCL_IGFX_PT);
        else if ( pci_gmch_id == CALPELLA_GMCH_ID )
            pm_quirks &= ~(PM_QUIRK_SW_ASSIST_BCL|PM_QUIRK_SW_ASSIST_BCL_IGFX_PT);
    }
    else if ( (strnicmp(manufacturer, MANUFACTURER_FUJITSU, strlen(MANUFACTURER_FUJITSU)) == 0) ||
              (strnicmp(manufacturer, MANUFACTURER_FUJTSU, strlen(MANUFACTURER_FUJTSU)) == 0) )
    {
        /* CP systems like the E780 needs SW assistance and SB system E781 also needs it. Don't know about
         * any others so turn it on in all cases except for PVMs.
         */
        pm_quirks &= ~(PM_QUIRK_SW_ASSIST_BCL_IGFX_PT);
    }
    else if ( strnicmp(manufacturer, MANUFACTURER_PANASONIC, strlen(MANUFACTURER_PANASONIC)) == 0 )
    {
        pm_quirks &= ~(PM_QUIRK_SW_ASSIST_BCL_IGFX_PT);
    }
    else if ( strnicmp(manufacturer, MANUFACTURER_TOSHIBA, strlen(MANUFACTURER_TOSHIBA)) == 0 )
    {
        if ( (pci_gmch_id == MONTEVINA_GMCH_ID) || (pci_gmch_id == CALPELLA_GMCH_ID) )
            pm_quirks &= ~(PM_QUIRK_SW_ASSIST_BCL|PM_QUIRK_SW_ASSIST_BCL_IGFX_PT);
        else
            pm_quirks &= ~(PM_QUIRK_SW_ASSIST_BCL_IGFX_PT);
    }
    else if ( strnicmp(manufacturer, MANUFACTURER_APPLE, strlen(MANUFACTURER_APPLE)) == 0 )
    {
        pm_quirks &= ~(PM_QUIRK_SW_ASSIST_BCL_IGFX_PT);
    }

    xcpmd_log(LOG_INFO, "Platform manufacturer: %s product: %s BIOS version: %s\n", manufacturer, product, bios_version);

out:
    if ( locator.addr != 0 )
        unmap_phys_mem(locator.addr, locator.length);
}

/* todo:
 * Eventually platform specs and quirk management will be moved to a central location (e.g. in
 * the config db and made available on dbus and xs). These values will will gobally available
 * for platform specific configurations. For now, the quirks are just being setup in xcpmd.
 */
void initialize_platform_info(void)
{
    uint32_t pci_val;
    uint16_t pci_vendor_id, pci_dev_id, pci_class_id;
    int battery_present, battery_total;

    if ( !pci_lib_init() )
    {
        xcpmd_log(LOG_ERR, "%s failed to initialize PCI utils library\n", __FUNCTION__);
        return;
    }

    /* Memset global vars */
    memset(wmi_pd, 0x0, sizeof (wmi_pd));
    memset(&invocation_data, 0x0, sizeof (invocation_data));
    memset(&hp_command_hotkeys_arg, 0x0, sizeof (hp_command_hotkeys_arg));

    /* Do setup stuffs */
    setup_software_bcl_and_input_quirks();
    setup_xenstore_platform_bcl_count();
    setup_hp_wmi_hotkeys_data_blocks();
    setup_wmi_platform_devices();
    /* NOTE: The OEM platform support feature is currently effectively dead
     * but a bunch of the feature's code is still hanging around in case
     * we ever need to bring the feature back. Unfortunately with recent
     * kernel updates, the creation of the WMI SSDT is causing errors and
     * failures. Stubbing this disabled that part of the feature.
     */
    /*setup_wmi_ssdt_external_file();*/

    /* Test for intel gpu - not dealing with multiple GPUs at the moment */
    pci_val = pci_host_read_dword(0, 2, 0, PCI_VENDOR_DEVICE_OFFSET);
    if ( pci_val != PCI_INVALID_VALUE )
    {
        pci_vendor_id = PCI_VENDOR_ID_WORD(pci_val);
        pci_dev_id = PCI_DEVICE_ID_WORD(pci_val);
        pci_class_id = PCI_CLASS_ID_WORD(pci_host_read_dword(0, 2, 0, PCI_CLASS_REV_OFFSET));
        if ( pci_class_id == PCI_VIDEO_VGA_CLASS_ID )
        {
            if ( pci_vendor_id == INTEL_VENDOR_ID )
                pm_specs |= PM_SPEC_INTEL_GPU;

            xcpmd_log(LOG_INFO, "Platform specs - GPU at 00:02.0 Vendor ID: %4.4x Device ID: %4.4x\n",
                      pci_vendor_id, pci_dev_id);
        }
        else
            xcpmd_log(LOG_INFO, "Platform specs - Device at 00:02.0 Class: %4.4x Vendor ID: %4.4x Device ID: %4.4x\n",
                      pci_class_id, pci_vendor_id, pci_dev_id);
    }
    else
        xcpmd_log(LOG_INFO, "Platform specs - no device at 00:02.0\n");

    if ( test_has_directio() == 1 )
        pm_specs |= PM_SPEC_DIRECT_IO;

    /* Open the battery files if they are present and setup the xenstore
     * battery information. Set the spec flag if there are no batteries.
     * Note that a laptop with no batteries connected still reports all its
     * battery slots but a desktop system will have an empty battery list.
     */
    battery_present = write_battery_info(&battery_total);

    if ( battery_total > 0 )
    {
        xenstore_write(battery_present ? "1" : "0", XS_BATTERY_PRESENT);

        xcpmd_log(LOG_INFO, "Battery information - total battery slots: %d  batteries present: %d\n",
            battery_total, battery_present);
    }
    else
    {
        xcpmd_log(LOG_INFO, "No batteries or battery slots on platform.\n");
        pm_specs |= PM_SPEC_NO_BATTERIES;
    }


    xcpmd_log(LOG_INFO, "Platform quirks: %8.8x specs: %8.8x\n", pm_quirks, pm_specs);

    pci_lib_cleanup();
}

void check_hp_hotkey_switch(void)
{
    struct wmi_bios_return *wmi_ret;
    void *out_buf;
    uint32_t out_len;
    int ret, err;

    if ( hp_hotkey_cmd == HP_HOTKEY_NONE )
        return;

#ifdef XCPMD_DEBUG
    xcpmd_log(LOG_DEBUG, "~Hotkey switch value %d processed\n", hp_hotkey_cmd);
#endif

    if ( hp_hotkey_cmd == HP_HOTKEY_SET )
        hp_command_hotkeys_arg.data = WMI_HP_DATA_HOTKEY_SET;
    else
        hp_command_hotkeys_arg.data = WMI_HP_DATA_HOTKEY_RESET;

    hp_hotkey_cmd = HP_HOTKEY_NONE;

    ret = xenacpi_wmi_invoke_method(&invocation_data,
                                    &hp_command_hotkeys_arg,
                                    sizeof(struct wmi_bios_args),
                                    &out_buf,
                                    &out_len,
                                    &err);
    if ( ret == -1 )
    {
        xcpmd_log(LOG_ERR, "Failed invocation of HP hotkey WMI method - error: %d\n", err);
        return;
    }

    if ( (out_buf != NULL) && (out_len >= sizeof(struct wmi_bios_return)) )
    {
        wmi_ret = (struct wmi_bios_return*)out_buf;
        if ( (wmi_ret->sigpass != WMI_HP_SIGRETURN_PASS) || (wmi_ret->return_code != 0) )
            xcpmd_log(LOG_ERR, "Return values indicate HP hotkey WMI method failed, sig: 0x%x code: 0x%x\n",
                wmi_ret->sigpass, wmi_ret->return_code);
    }
    else
        xcpmd_log(LOG_ERR, "Failed HP hotkey WMI method, invalid output buffer(s)?\n");

    if ( out_buf != NULL )
        xenacpi_free_buffer(out_buf);
}

struct wmi_platform_device* check_wmi_platform_device(const char *busid)
{
    int i;

    for ( i = 0; i < WMI_MAX_PLATFORM_DEVICES; i++ )
    {
        if ((wmi_pd[i].name[0] == '\0') && (wmi_pd[i].wmiid == 0))
            break;
        if (strcmp(busid, wmi_pd[i].bus_name) == 0)
            return &wmi_pd[i];
    }

    return NULL;
}
