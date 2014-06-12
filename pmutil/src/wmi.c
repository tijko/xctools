/*
 * wmi.c
 *
 * PM WMI routines.
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
#include <sys/stat.h>
#include <time.h>
#include "pmutil.h"

#define WMI_DEFAULT_BUFFER 1024

#define WMI_INVOCATION "WMI-INVOCATION: "

#define WMIACPI_REGFLAG_EXPENSIVE   0x01
#define WMIACPI_REGFLAG_METHOD      0x02
#define WMIACPI_REGFLAG_STRING      0x04
#define WMIACPI_REGFLAG_EVENT       0x08

#define WMI_FLAG_METHOD_CALL        0x00000001
#define WMI_FLAG_QUERY_OBJ          0x00000002
#define WMI_FLAG_SET_OBJ            0x00000004

struct wmi_invocation_block {
    uint8_t guid[XENACPI_WMI_GUID_SIZE];
    uint32_t wmiid;
    uint32_t flags;
    uint32_t instance;
    uint32_t method_id;
    uint32_t in_length;
    /* n bytes of input data */
} __attribute__ ((packed));

static void wmi_print_guid_block(struct xenacpi_wmi_guid_block *wgblock)
{
    const char *type;
    char objid[17];
    int ev = 0;
    uint8_t *pg = &wgblock->guid[0];

    memset(objid, 0, 16);

    if ( wgblock->flags & WMIACPI_REGFLAG_METHOD )
    {
        type = "METHOD";
        sprintf(objid, "WM%c%c",
            wgblock->wmi_type.object_id[0], wgblock->wmi_type.object_id[1]);
    }
    else if ( wgblock->flags & WMIACPI_REGFLAG_EVENT )
    {
        type = "EVENT";
        ev = 1;
    }
    else if ( memcmp(pg, XENACPI_WMI_MOF_GUID, XENACPI_WMI_GUID_SIZE) != 0 )
    {
        type = "DATA-BLOCK";
        sprintf(objid, "WQ%c%c-WS%c%c",
            wgblock->wmi_type.object_id[0], wgblock->wmi_type.object_id[1],
            wgblock->wmi_type.object_id[0], wgblock->wmi_type.object_id[1]);
    }
    else
    {
        type = "DATA-BLOCK";
        sprintf(objid, "WQ%c%c (MOF block)",
            wgblock->wmi_type.object_id[0], wgblock->wmi_type.object_id[1]);
    }

    printf("  Type: %s\n", type);

    if ( !ev )
        printf("  Object ID: %s\n", objid);
    else
        printf("  Notify ID: 0x%2.2x\n", wgblock->wmi_type.wmi_event.notify_id);

    printf("  Instance Count: 0x%2.2x\n", wgblock->instance_count);

    printf("  Flags: 0x%2.2x (Expensive: %s String: %s)\n", wgblock->flags,
        (wgblock->flags & WMIACPI_REGFLAG_EXPENSIVE ? "True" : "False"),
        (wgblock->flags & WMIACPI_REGFLAG_STRING ? "True" : "False"));

    printf("  GUID: %2.2x-%2.2x-%2.2x-%2.2x-%2.2x-%2.2x-%2.2x-%2.2x\n",
        pg[0], pg[1], pg[2], pg[3], pg[4], pg[5], pg[6], pg[7]);
    printf("        %2.2x-%2.2x-%2.2x-%2.2x-%2.2x-%2.2x-%2.2x-%2.2x\n",
        pg[8], pg[9], pg[10], pg[11], pg[12], pg[13], pg[14], pg[15]);
}

void wmi_list_devices(void)
{    
    struct xenacpi_wmi_device *wdevices = NULL;
    struct xenacpi_wmi_guid_block *gblocks = NULL;
    uint32_t count, i, gcount, j;
    char name[16];
    int ret, err;

    ret = xenacpi_wmi_get_devices(&wdevices, &count, &err);
    if ( ret == -1 )
    {
        fprintf(stderr, "WMI: failed to get WMI device listing, error: %d\n", err);
        return;
    }

    /* write out a summary first */
    printf("WMI ACPI Device Listing\n\n");
    printf("---------------Summary---------------\n");
    for ( i = 0; i < count; i++ )
    {
        printf("WMIID: %d\n", (int)wdevices[i].wmiid);
        xenacpi_wmi_extract_name(name, wdevices[i].name);
        printf("NAME:  %s\n", name);
        printf("_HID:  %s\n", wdevices[i]._hid);
        printf("_UID:  %s\n\n", wdevices[i]._uid);
    }

    printf("----------------GUIDs----------------\n");
    for ( i = 0; i < count; i++ )
    {
        ret = xenacpi_wmi_get_device_blocks(wdevices[i].wmiid,
                                            &gblocks,
                                            &gcount,
                                            &err);
        if ( ret == -1 )
        {
            fprintf(stderr, "WMI: failed to get WMI block for WMIID: %d, error: %d\n",
                    (int)wdevices[i].wmiid, err);
            break;
        }

        printf("GUID-BLOCK list for WMIID: %d\n", (int)wdevices[i].wmiid);
        for ( j = 0; j < gcount; j++ )
        {
            wmi_print_guid_block(&gblocks[j]);
            printf("\n");
        }

        printf("\n");
        xenacpi_free_buffer(gblocks);
        gblocks = NULL;
    }

    if ( gblocks != NULL )
        xenacpi_free_buffer(gblocks);

    if ( wdevices != NULL )
        xenacpi_free_buffer(wdevices);
}

void wmi_write_mof(uint32_t wmiid)
{
    struct xenacpi_wmi_device *wdevices = NULL;
    struct xenacpi_wmi_guid_block *gblocks = NULL;
    struct xenacpi_wmi_invocation_data invocation_data = {0};
    void *mof_data = NULL;
    uint32_t count, gcount, mof_len, i;
    FILE *fd = NULL;
    char name[16];
    char objid[16];
    char id[32];
    char file_name[128];
    int ret, err;

    ret = xenacpi_wmi_get_devices(&wdevices, &count, &err);
    if ( ret == -1 )
    {
        fprintf(stderr, "WMI: failed to get WMI device for WMIID: %d, error: %d\n",
                (int)wmiid, err);
        return;
    }

    /* find a match */
    for ( i = 0; i < count; i++ )
    {
        if ( wdevices[i].wmiid == wmiid )
            break;
    }

    if ( i == count )
    {
        printf("Could not find device with WMIID: %d\n", (int)wmiid);
        goto wmi_out;
    }

    memset(id, 0, 32);
    sprintf(id, "%d", (int)wmiid);
    xenacpi_wmi_extract_name(name, wdevices[i].name);

    ret = xenacpi_wmi_get_device_blocks(wmiid, &gblocks, &gcount, &err);
    if ( ret == -1 )
    {
        fprintf(stderr, "WMI: failed to get WMI block for WMIID: %d, error: %d\n",
                (int)wmiid, err);
        goto wmi_out;
    }

    for ( i = 0; i < gcount; i++ )
    {
        if ( memcmp(&gblocks[i].guid[0], XENACPI_WMI_MOF_GUID, XENACPI_WMI_GUID_SIZE) == 0 )
            break;
    }

    if ( i == count )
    {
        printf("Could not find MOF for device with WMIID: %d\n", (int)wmiid);
        goto wmi_out;
    }

    if ( (gblocks[i].flags & WMIACPI_REGFLAG_METHOD) || (gblocks[i].flags & WMIACPI_REGFLAG_EVENT) )
    {
        printf("Invalid flags 0x%2.2x for MOF\n", gblocks[i].flags);
        goto wmi_out;
    }

    sprintf(objid, "WQ%c%c", gblocks[i].wmi_type.object_id[0], gblocks[i].wmi_type.object_id[1]);

    /* invoke query for the MOF data block */
    memcpy(&invocation_data.guid[0], XENACPI_WMI_MOF_GUID, XENACPI_WMI_GUID_SIZE);
    invocation_data.objid[0] = gblocks[i].wmi_type.object_id[0];
    invocation_data.objid[1] = gblocks[i].wmi_type.object_id[1];
    invocation_data.flags = XENACPI_WMI_FLAG_USE_OBJID;
    invocation_data.instance = 0x1;

    ret = xenacpi_wmi_query_object(&invocation_data, &mof_data, &mof_len, &err);
    if ( ret == -1 )
    {
        fprintf(stderr, "WMI: failed to query MOF for WMIID: %d, error: %d\n",
                (int)wmiid, err);
        goto wmi_out;
    }

    /* write out the MOF data to a file in the current dir */
    memset(file_name, 0, 128);
    sprintf(file_name, "./MOF-%s-WMIID%s-%s.bmf", name, id, objid);

    fd = fopen(file_name, "wb");
    if ( fd == NULL )
    {
        printf("Open output file %s failed - errno: %d\n", file_name, errno);
        goto wmi_out;
    }

    ret = fwrite(mof_data, 1, mof_len, fd);
    if ( ret != mof_len )
    {
        printf("Failed to write MOF data to file %s - error: %d!\n", file_name, ferror(fd));
        goto wmi_out;
    }

    printf("MOF data for WMMID %d written to file %s\n", (int)wmiid, file_name);

wmi_out:
    if ( fd != NULL )
        fclose(fd);

    if ( mof_data != NULL )
        xenacpi_free_buffer(mof_data);

    if ( gblocks != NULL )
        xenacpi_free_buffer(gblocks);

    if ( wdevices != NULL )
        xenacpi_free_buffer(wdevices);
}

void wmi_invoke(char *wmi_file)
{
    struct stat wmi_stat;
    FILE *fd = NULL;
    uint8_t *inbuf = NULL;
    uint8_t *pg;
    struct wmi_invocation_block *wmi_block;
    struct xenacpi_wmi_invocation_data invocation_data = {0};
    void *out_buf = NULL;
    uint32_t out_len;
    int ret, err;
    char file_name[128];

    if ( stat(wmi_file, &wmi_stat) < 0 )
    {
        printf("Stat input file %s failed - errno: %d\n", wmi_file, errno);
        return;
    }

    if ( wmi_stat.st_size < sizeof(struct wmi_invocation_block) )
    {
        printf("Input file %s is too small to contain invocation data\n", wmi_file);
        return;
    }

    inbuf = (uint8_t*)malloc(wmi_stat.st_size);
    if ( inbuf == NULL )
    {
        printf("Could not alloc buffer for input data, out of memory\n");
        return;
    }

    printf(WMI_INVOCATION "Reading input file: %s size: %d\n", wmi_file, wmi_stat.st_size);

    fd = fopen(wmi_file, "rb");
    if ( fd == NULL )
    {
        printf("Open input file %s failed - errno: %d\n", wmi_file, errno);
        goto wmi_out;
    }

    if ( fread(inbuf, wmi_stat.st_size, 1, fd) < 1 )
    {
        printf("Read input file %s failed - ferror: %d feof: %d\n", wmi_file, ferror(fd), feof(fd));
        goto wmi_out;
    }
    wmi_block = (struct wmi_invocation_block*)inbuf;
    pg = &wmi_block->guid[0];
    fclose(fd);
    fd = NULL;

    if ( wmi_block->in_length > (wmi_stat.st_size - sizeof(struct wmi_invocation_block)) )
    {
        printf("Input data length %s is greater that size of data in the file\n", wmi_block->in_length);
        goto wmi_out;
    }

    printf(WMI_INVOCATION "Input values:\n");
    printf(WMI_INVOCATION "  GUID: %2.2x-%2.2x-%2.2x-%2.2x-%2.2x-%2.2x-%2.2x-%2.2x\n",
        pg[0], pg[1], pg[2], pg[3], pg[4], pg[5], pg[6], pg[7]);
    printf(WMI_INVOCATION "        %2.2x-%2.2x-%2.2x-%2.2x-%2.2x-%2.2x-%2.2x-%2.2x\n",
        pg[8], pg[9], pg[10], pg[11], pg[12], pg[13], pg[14], pg[15]);
    printf(WMI_INVOCATION "  WMIID:     %d\n", (int)wmi_block->wmiid);
    printf(WMI_INVOCATION "  Flags:     0x%8.8x\n", wmi_block->flags);
    printf(WMI_INVOCATION "  Instance:  %d\n", (int)wmi_block->instance);
    printf(WMI_INVOCATION "  Method ID: %d\n", (int)wmi_block->method_id);
    printf(WMI_INVOCATION "  Length:    %d (0x%x)\n", (int)wmi_block->in_length, wmi_block->in_length);

    memcpy(&invocation_data.guid[0], &wmi_block->guid[0], XENACPI_WMI_GUID_SIZE);
    invocation_data.wmiid = wmi_block->wmiid;
    invocation_data.flags = XENACPI_WMI_FLAG_USE_WMIID;
    invocation_data.instance = wmi_block->instance;

    if ( wmi_block->flags & WMI_FLAG_METHOD_CALL )
    {        
        invocation_data.method_id = wmi_block->method_id;
        ret = xenacpi_wmi_invoke_method(&invocation_data,
                                        (inbuf + sizeof(struct wmi_invocation_block)),
                                        (uint32_t)wmi_block->in_length,
                                        &out_buf,
                                        &out_len,
                                        &err);
    }
    else if ( wmi_block->flags & WMI_FLAG_QUERY_OBJ )
    {
        ret = xenacpi_wmi_query_object(&invocation_data,
                                       &out_buf,
                                       &out_len,
                                       &err);
    }
    else if ( wmi_block->flags & WMI_FLAG_SET_OBJ )
    {
        ret = xenacpi_wmi_set_object(&invocation_data,
                                     (inbuf + sizeof(struct wmi_invocation_block)),
                                     (uint32_t)wmi_block->in_length,
                                     &err);        
    }
    else
    {
        printf("Invalid invocation flags %d\n", wmi_block->flags);
        goto wmi_out;
    }

    if ( ret == -1 )
    {
        fprintf(stderr, "WMI: Invocation failed, error: %d\n", err);
        goto wmi_out;
    }

    if ( out_buf == NULL )
        goto wmi_out;

    /* write output to a file */
    memset(file_name, 0, 128);
    sprintf(file_name, "./WMIID%d-OUTPUT-%d.bin", (int)wmi_block->wmiid, (int)time(NULL));

    fd = fopen(file_name, "wb");
    if ( fd == NULL )
    {
        printf("Open output file %s failed - errno: %d\n", file_name, errno);
        goto wmi_out;
    }

    ret = fwrite(out_buf, 1, out_len, fd);
    if ( ret != out_len )
    {
        printf("Failed to write output data to file %s - error: %d!\n", file_name, ferror(fd));
        goto wmi_out;
    }

    printf("Output data for WMIID %d written to file %s\n", (int)wmi_block->wmiid, file_name);

wmi_out:
    if ( out_buf != NULL )
        xenacpi_free_buffer(out_buf);

    if ( fd != NULL )
        fclose(fd);

    if ( inbuf != NULL )
        free(inbuf);
}
