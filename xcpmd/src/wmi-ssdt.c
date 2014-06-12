/*
 * wmi-ssdt.c
 *
 * XenClient platform management daemon WMI SSDT generation module.
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

#ifndef XENAML_TEST_APP
#include "project.h"
#include "xcpmd.h"
#else
#include "project_test.h"
#endif

#define WMI_PAGE_ALIGN(x, p) ((x + (p - 1)) & ~(p - 1))

struct wmi_device {
    uint32_t wmiid;
    char name[XENAML_NAME_SIZE + 1];
    char _uid[XENACPI_WMI_NAME_SIZE + 1];

    int skip;

    struct xenacpi_wmi_guid_block *gblocks;
    uint32_t gcount;

    uint8_t *mofdata;
    uint32_t mofsize;
};

struct wmi_context {
    struct wmi_device *devices;
    uint32_t count;

    void *pma;
    uint32_t pmsize;
};

static void wmi_delete_devices(struct wmi_device *devices,
                               uint32_t count)
{
    uint32_t i;

    for ( i = 0; i < count; i++ )
    {
        if ( devices[i].gblocks != NULL )
            free(devices[i].gblocks);
        if ( devices[i].mofdata != NULL )
            free(devices[i].mofdata);
    }
    free(devices);
}

static int wmi_get_devices(struct wmi_device **devices_out,
                           uint32_t *count_out)
{
    struct xenacpi_wmi_device *wdevices = NULL;
    struct xenacpi_wmi_guid_block *gblocks = NULL;
    struct xenacpi_wmi_invocation_data invocation_data;
    struct wmi_device *devices = NULL;
    void *mofdata = NULL;
    uint32_t count, i, gcount, moflen;
    int ret, err = 0;

    *devices_out = NULL;
    *count_out = 0;

    memset(&invocation_data, 0x0, sizeof (invocation_data));

    ret = xenacpi_wmi_get_devices(&wdevices, &count, &err);
    if ( ret == -1 )
    {
        xcpmd_log(LOG_ERR, "%s failed to get WMI devices, error: %d\n",
                  __FUNCTION__, err);
        return -1;
    }

    devices = malloc(count*sizeof(struct wmi_device));
    if ( devices == NULL )
    {
        xcpmd_log(LOG_ERR, "%s failed to allocate device array\n",
                  __FUNCTION__);
        goto err_out;
    }
    memset(devices, 0, count*sizeof(struct wmi_device));

    for ( i = 0; i < count; i++ )
    {
        /* Reset */
        gblocks = NULL;
        mofdata = NULL;
        err = 0;

        /* Get basic information for device */
        devices[i].wmiid = wdevices[i].wmiid;
        memcpy(devices[i]._uid, wdevices[i]._uid, XENACPI_WMI_NAME_SIZE);
        /* Making an assumption that these will always be 4 char std ACPI names */
        xenacpi_wmi_extract_name(devices[i].name, wdevices[i].name);

        /* Check for WMI devices we want to skip. I hate to put platform specific logic here but
         * occasionally there are WMI devices that we don't really want. At the moment there is
         * only one that is known on the HP 8440p. There is a WMI device with a _UID of NVIF that
         * is tied to the graphics device on the platform. It is not clear what the device is used
         * for but we never surfaced it in a guest before and it seems to cause issues.
         */
        if ( memcmp(devices[i]._uid, "NVIF", 4) == 0 )
        {
            xcpmd_log(LOG_INFO, "%s skipping WMI device _UID(NVIF) with WMIID %d\n",
                      __FUNCTION__, wdevices[i].wmiid);
            devices[i].skip = 1;
            continue;
        }

        /* Get the GUID blocks that make up the _WDG and copy it*/
        ret = xenacpi_wmi_get_device_blocks(wdevices[i].wmiid, &gblocks, &gcount, &err);
        if ( ret == -1 )
        {
            xcpmd_log(LOG_ERR, "%s failed to get WMI GUID block for WMIID %d, error: %d\n",
                      __FUNCTION__, wdevices[i].wmiid, err);
            goto err_out;
        }
        devices[i].gblocks = malloc(gcount*sizeof(struct xenacpi_wmi_guid_block));
        if ( devices[i].gblocks == NULL )
        {
            xcpmd_log(LOG_ERR, "%s failed to allocate gblock array\n",
                      __FUNCTION__);
            goto err_out;
        }
        memcpy(devices[i].gblocks, gblocks, gcount*sizeof(struct xenacpi_wmi_guid_block));
        devices[i].gcount = gcount;
        xenacpi_free_buffer(gblocks);
        gblocks = NULL;

        /* Get the MOF data for this device and store it */
        memcpy(&invocation_data.guid[0], XENACPI_WMI_MOF_GUID, XENACPI_WMI_GUID_SIZE);
        invocation_data.wmiid = devices[i].wmiid;
        invocation_data.flags = XENACPI_WMI_FLAG_USE_WMIID;
        invocation_data.instance = 0x1;

        ret = xenacpi_wmi_query_object(&invocation_data, &mofdata, &moflen, &err);
        if ( ret == -1 )
        {
            xcpmd_log(LOG_ERR, "%s failed to query MOF for WMIID: %d, error: %d\n",
                      __FUNCTION__, (int)devices[i].wmiid, err);
            goto err_out;
        }
        devices[i].mofdata = malloc(moflen);
        if ( devices[i].mofdata == NULL )
        {
            xcpmd_log(LOG_ERR, "%s failed to allocate MOF data\n",
                      __FUNCTION__);
            goto err_out;
        }
        memcpy(devices[i].mofdata, mofdata, moflen);
        devices[i].mofsize = moflen;
        xenacpi_free_buffer(mofdata);
        mofdata = NULL;
    }

    xenacpi_free_buffer(wdevices);
    *devices_out = devices;
    *count_out = count;

    return 0;
err_out:
    if ( mofdata != NULL )
        xenacpi_free_buffer(mofdata);

    if ( gblocks != NULL )
        xenacpi_free_buffer(gblocks);

    if ( devices != NULL )
        wmi_delete_devices(devices, count);

    if ( wdevices != NULL )
        xenacpi_free_buffer(wdevices);

    return -1;
}

static int wmi_create_context(struct wmi_context **context_out)
{
    struct wmi_context *ctx;
    uint32_t sc_pagesize = (uint32_t)sysconf(_SC_PAGESIZE);
    uint32_t i;
    int ret;

    *context_out = NULL;

    ctx = malloc(sizeof(struct wmi_context));
    if ( ctx == NULL )
    {
        xcpmd_log(LOG_ERR, "%s failed to allocate AML context block\n",
                  __FUNCTION__);
        return -1;
    }
    memset(ctx, 0, sizeof(struct wmi_context));

    ret = wmi_get_devices(&ctx->devices, &ctx->count);
    if ( ret != 0 )
        goto err_out;

    /* Calculate how much pre-alloced memory to create for the AML library */
    ctx->pmsize = 4*sc_pagesize; /* basic amount for the WIF1 device and static bits */

    for ( i = 0; i < ctx->count; i++ )
    {
        /* 4 more for each device */
        ctx->pmsize += 4*sc_pagesize;
        /* some pages for the MOF which is by far the biggest bit of a WMI device */
        ctx->pmsize += WMI_PAGE_ALIGN((ctx->devices[i].mofsize), sc_pagesize);
    }

    /* Make me some of that buffer stuffs */
    ctx->pma = xenaml_create_premem(ctx->pmsize);
    if ( ctx->pma == NULL )
    {
        xcpmd_log(LOG_ERR, "%s failed to allocate AML library premem\n",
                  __FUNCTION__);
        goto err_out;
    }
    *context_out = ctx;

    return 0;
err_out:
    if ( ctx != NULL )
    {
        if ( ctx->devices != NULL )
            wmi_delete_devices(ctx->devices, ctx->count);
        free(ctx);
    }

    return -1;
}

static void wmi_destroy_context(struct wmi_context *ctx)
{
    wmi_delete_devices(ctx->devices, ctx->count);
    xenaml_free_premem(ctx->pma);
    free(ctx);
}

static void wmi_chain_peers(void **current, void **next)
{
    int ret, err = 0;

    /* Note below that the call to chain nodes together is assumed to
     * succeed. This is because the can not fail unless there is a bug
     * earlier on in building the nodes. The chain routines only fail if
     * the nodes are not in a proper state.
     */
    ret = xenaml_chain_peers(*current, *next, &err);
    assert((ret == 0)&&(err == 0));

    /* Move the nodes forward so the caller can just keep calling this
     * routine and stringing on nodes.
     */
    *current = *next;
}

static void* wmi_opregions(struct wmi_context *ctx, void **last)
{
    struct xenaml_field_unit ens[4];
    void *first = NULL, *current, *next;

    *last = NULL;

    /* Make first operation region and field:
     * OperationRegion (WOR1, SystemIO, 0x96, 0x04)
     * Field (WOR1, ByteAcc, NoLock, Preserve)
     * {
     *     P96,   8,
     *     P97,   8,
     *     P98,   8,
     *     P99,   8
     * }
     */
    current = xenaml_op_region("WOR1",
                               XENAML_ADR_SPACE_SYSTEM_IO,
                               XENACPI_WMI_CMD_PORT,
                               0x4,
                               ctx->pma);
    first = current;

    memcpy(&ens[0].aml_field.aml_name.name[0], "P96_", XENAML_NAME_SIZE);
    ens[0].aml_field.aml_name.size_in_bits = 0x8;
    ens[0].type = XENAML_FIELD_TYPE_NAME;

    memcpy(&ens[1].aml_field.aml_name.name[0], "P97_", XENAML_NAME_SIZE);
    ens[1].aml_field.aml_name.size_in_bits = 0x8;
    ens[1].type = XENAML_FIELD_TYPE_NAME;

    memcpy(&ens[2].aml_field.aml_name.name[0], "P98_", XENAML_NAME_SIZE);
    ens[2].aml_field.aml_name.size_in_bits = 0x8;
    ens[2].type = XENAML_FIELD_TYPE_NAME;

    memcpy(&ens[3].aml_field.aml_name.name[0], "P99_", XENAML_NAME_SIZE);
    ens[3].aml_field.aml_name.size_in_bits = 0x8;
    ens[3].type = XENAML_FIELD_TYPE_NAME;

    next = xenaml_field("WOR1",
                        XENAML_FIELD_ACCESS_TYPE_BYTE,
                        XENAML_FIELD_LOCK_NEVER,
                        XENAML_FIELD_UPDATE_PRESERVE,
                        &ens[0],
                        4,
                        ctx->pma);
    wmi_chain_peers(&current, &next);

    /* Make second operation region and field:
     * OperationRegion (WOR2, SystemIO, 0x9A, 0x04)
     * Field (WOR2, DWordAcc, NoLock, Preserve)
     * {
     *     P9A,   8
     * }
     */
    next = xenaml_op_region("WOR2",
                            XENAML_ADR_SPACE_SYSTEM_IO,
                            XENACPI_WMI_DATA_PORTL,
                            0x4,
                            ctx->pma);
    wmi_chain_peers(&current, &next);

    memcpy(&ens[0].aml_field.aml_name.name[0], "P9A_", XENAML_NAME_SIZE);
    ens[0].aml_field.aml_name.size_in_bits = 0x20;
    ens[0].type = XENAML_FIELD_TYPE_NAME;

    next = xenaml_field("WOR2",
                        XENAML_FIELD_ACCESS_TYPE_DWORD,
                        XENAML_FIELD_LOCK_NEVER,
                        XENAML_FIELD_UPDATE_PRESERVE,
                        &ens[0],
                        1,
                        ctx->pma);
    wmi_chain_peers(&current, &next);

    *last = current;
    return first;
}

static void* wif1_store_methods(struct wmi_context *ctx,
                                const char *method_name,
                                enum XENACPI_WMI_COMMAND ordinal,
                                const char *port1,
                                const char *port2)
{
    void *first = NULL, *current, *next;
    struct xenaml_args al;
    uint8_t args = 0;

    memset(&al, 0x0, sizeof (al));
    al.arg[0] = xenaml_integer(ordinal, XENAML_INT_OPTIMIZE, ctx->pma);
    al.arg[1] = xenaml_name_reference(port1, NULL, ctx->pma);
    al.count = 2;
    current = xenaml_misc(XENAML_MISC_FUNC_STORE, &al, ctx->pma);
    first = current;

    if ( ordinal != XENACPI_WMI_CMD_EXECUTE )
    {
        al.arg[0] = xenaml_variable(XENAML_VARIABLE_TYPE_ARG, 0, ctx->pma);
        al.arg[1] = xenaml_name_reference(port2, NULL, ctx->pma);
        al.count = 2;
        next = xenaml_misc(XENAML_MISC_FUNC_STORE, &al, ctx->pma);
        wmi_chain_peers(&current, &next);
        args++;
    }

    return xenaml_method(method_name, args, 1, first, ctx->pma);
}

static void* wif1_guid_method(struct wmi_context *ctx)
{
    void *condition = NULL;
    void *loop = NULL;
    void *first = NULL, *current, *next;
    struct xenaml_args al;

    memset(&al, 0x0, sizeof (al));

    /* Build the loop condition first */
    /* LLess (Local0, 16) */
    al.arg[0] = xenaml_variable(XENAML_VARIABLE_TYPE_LOCAL, 0, ctx->pma);
    al.arg[1] = xenaml_integer(16, XENAML_INT_BYTE, ctx->pma);
    al.count = 2;
    condition = xenaml_logic(XENAML_LOGIC_FUNC_LESSTHAN, &al, ctx->pma);

    /* Now the loop body */
    /* Add (Local1, Local0, Local2) */
    al.arg[0] = xenaml_variable(XENAML_VARIABLE_TYPE_LOCAL, 1, ctx->pma);
    al.arg[1] = xenaml_variable(XENAML_VARIABLE_TYPE_LOCAL, 0, ctx->pma);
    al.arg[2] = xenaml_variable(XENAML_VARIABLE_TYPE_LOCAL, 2, ctx->pma);
    al.count = 3;
    current = xenaml_math(XENAML_MATH_FUNC_ADD, &al, ctx->pma);
    first = current;

    /* Index (Arg1, Local2, 0) */
    al.arg[0] = xenaml_variable(XENAML_VARIABLE_TYPE_ARG, 1, ctx->pma);
    al.arg[1] = xenaml_variable(XENAML_VARIABLE_TYPE_LOCAL, 2, ctx->pma);
    al.arg[2] = xenaml_integer(0, XENAML_INT_ZERO, ctx->pma);
    al.count = 3;
    next = xenaml_misc(XENAML_MISC_FUNC_INDEX, &al, ctx->pma);

    /* DerefOf (Index (Arg1, Local2, 0)) */
    al.arg[0] = next;
    al.count = 1;
    next = xenaml_misc(XENAML_MISC_FUNC_DEREFOF, &al, ctx->pma);

    /* Store (DerefOf(Index (Arg1, Local2, 0)), P98 ) */
    al.arg[0] = next;
    al.arg[1] = xenaml_name_reference("P98_", NULL, ctx->pma);
    al.count = 2;
    next = xenaml_misc(XENAML_MISC_FUNC_STORE, &al, ctx->pma);
    wmi_chain_peers(&current, &next);

    /* Increment (Local0) */
    al.arg[0] = xenaml_variable(XENAML_VARIABLE_TYPE_LOCAL, 0, ctx->pma);
    al.count = 1;
    next = xenaml_math(XENAML_MATH_FUNC_INCREMENT, &al, ctx->pma);
    wmi_chain_peers(&current, &next);

    loop = xenaml_while(condition, first, ctx->pma);

    /* Build the first part of the method */
    /* Store (101, P96) */
    al.arg[0] = xenaml_integer(XENACPI_WMI_CMD_GUID, XENAML_INT_OPTIMIZE, ctx->pma);
    al.arg[1] = xenaml_name_reference("P96_", NULL, ctx->pma);
    al.count = 2;
    current = xenaml_misc(XENAML_MISC_FUNC_STORE, &al, ctx->pma);
    first = current;

    /* Store (0x0, Local0) */
    al.arg[0] = xenaml_integer(0, XENAML_INT_ZERO, ctx->pma);
    al.arg[1] = xenaml_variable(XENAML_VARIABLE_TYPE_LOCAL, 0, ctx->pma);
    al.count = 2;
    next = xenaml_misc(XENAML_MISC_FUNC_STORE, &al, ctx->pma);
    wmi_chain_peers(&current, &next);

    /* Store (Arg0, Local1) */
    al.arg[0] = xenaml_variable(XENAML_VARIABLE_TYPE_ARG, 0, ctx->pma);
    al.arg[1] = xenaml_variable(XENAML_VARIABLE_TYPE_LOCAL, 1, ctx->pma);
    al.count = 2;
    next = xenaml_misc(XENAML_MISC_FUNC_STORE, &al, ctx->pma);
    wmi_chain_peers(&current, &next);

    /* Chain the loop on at the end */
    wmi_chain_peers(&current, &loop);

    return xenaml_method("GUID", 2, 1, first, ctx->pma);
}

static void* wif1_ibuf_method(struct wmi_context *ctx)
{
    void *condition = NULL;
    void *loop = NULL;
    void *first = NULL, *current, *next;
    struct xenaml_args al;

    memset(&al, 0x0, sizeof (al));

    /* Build the loop condition first */
    /* LLess (Local2, Local0) */
    al.arg[0] = xenaml_variable(XENAML_VARIABLE_TYPE_LOCAL, 2, ctx->pma);
    al.arg[1] = xenaml_variable(XENAML_VARIABLE_TYPE_LOCAL, 0, ctx->pma);
    al.count = 2;
    condition = xenaml_logic(XENAML_LOGIC_FUNC_LESSTHAN, &al, ctx->pma);

    /* Now the loop body */
    /* Index (Local1, Local2, 0) */
    al.arg[0] = xenaml_variable(XENAML_VARIABLE_TYPE_LOCAL, 1, ctx->pma);
    al.arg[1] = xenaml_variable(XENAML_VARIABLE_TYPE_LOCAL, 2, ctx->pma);
    al.arg[2] = xenaml_integer(0, XENAML_INT_ZERO, ctx->pma);
    al.count = 3;
    next = xenaml_misc(XENAML_MISC_FUNC_INDEX, &al, ctx->pma);

    /* DerefOf (Index (Local1, Local2)) */
    al.arg[0] = next;
    al.count = 1;
    next = xenaml_misc(XENAML_MISC_FUNC_DEREFOF, &al, ctx->pma);

    /* Store (DerefOf (Index (Local1, Local2)), P98) */
    al.arg[0] = next;
    al.arg[1] = xenaml_name_reference("P98_", NULL, ctx->pma);
    al.count = 2;
    current = xenaml_misc(XENAML_MISC_FUNC_STORE, &al, ctx->pma);
    first = current;

    /* Increment (Local2) */
    al.arg[0] = xenaml_variable(XENAML_VARIABLE_TYPE_LOCAL, 2, ctx->pma);
    al.count = 1;
    next = xenaml_math(XENAML_MATH_FUNC_INCREMENT, &al, ctx->pma);
    wmi_chain_peers(&current, &next);

    loop = xenaml_while(condition, first, ctx->pma);

    /* Build the first part of the method */
    /* Store (105, P96) */
    al.arg[0] = xenaml_integer(XENACPI_WMI_CMD_IN_BUFFER_SIZE, XENAML_INT_OPTIMIZE, ctx->pma);
    al.arg[1] = xenaml_name_reference("P96_", NULL, ctx->pma);
    al.count = 2;
    current = xenaml_misc(XENAML_MISC_FUNC_STORE, &al, ctx->pma);
    first = current;

    /* SizeOf (Arg0) */
    al.arg[0] = xenaml_variable(XENAML_VARIABLE_TYPE_ARG, 0, ctx->pma);
    al.count = 1;
    next = xenaml_misc(XENAML_MISC_FUNC_SIZEOF, &al, ctx->pma);

    /* Store (SizeOf (Arg0), Local0) */
    al.arg[0] = next;
    al.arg[1] = xenaml_variable(XENAML_VARIABLE_TYPE_LOCAL, 0, ctx->pma);
    al.count = 2;
    next = xenaml_misc(XENAML_MISC_FUNC_STORE, &al, ctx->pma);
    wmi_chain_peers(&current, &next);

    /* Store (Local0, P9A) */
    al.arg[0] = xenaml_variable(XENAML_VARIABLE_TYPE_LOCAL, 0, ctx->pma);
    al.arg[1] = xenaml_name_reference("P9A_", NULL, ctx->pma);
    al.count = 2;
    next = xenaml_misc(XENAML_MISC_FUNC_STORE, &al, ctx->pma);
    wmi_chain_peers(&current, &next);

    /* ToBuffer (Arg0, Local1) */
    al.arg[0] = xenaml_variable(XENAML_VARIABLE_TYPE_ARG, 0, ctx->pma);
    al.arg[1] = xenaml_variable(XENAML_VARIABLE_TYPE_LOCAL, 1, ctx->pma);
    al.count = 2;
    next = xenaml_misc(XENAML_MISC_FUNC_TOBUFFER, &al, ctx->pma);
    wmi_chain_peers(&current, &next);

    /* Store (0, Local2)) */
    al.arg[0] = xenaml_integer(0, XENAML_INT_ZERO, ctx->pma);
    al.arg[1] = xenaml_variable(XENAML_VARIABLE_TYPE_LOCAL, 2, ctx->pma);
    al.count = 2;
    next = xenaml_misc(XENAML_MISC_FUNC_STORE, &al, ctx->pma);
    wmi_chain_peers(&current, &next);

    /* Store (104, P96) */
    al.arg[0] = xenaml_integer(XENACPI_WMI_CMD_IN_BUFFER, XENAML_INT_OPTIMIZE, ctx->pma);
    al.arg[1] = xenaml_name_reference("P96_", NULL, ctx->pma);
    al.count = 2;
    next = xenaml_misc(XENAML_MISC_FUNC_STORE, &al, ctx->pma);
    wmi_chain_peers(&current, &next);

    /* Chain the loop on at the end */
    wmi_chain_peers(&current, &loop);

    return xenaml_method("IBUF", 1, 1, first, ctx->pma);
}

static void* wif1_obuf_method(struct wmi_context *ctx)
{
    void *condition = NULL;
    void *loop = NULL;
    void *first = NULL, *current, *next;
    struct xenaml_args al;
    struct xenaml_buffer_init binit;

    memset(&al, 0x0, sizeof (al));
    memset(&binit, 0x0, sizeof (binit));

    /* Build the loop condition first */
    /* LLess (Local1, Local0) */
    al.arg[0] = xenaml_variable(XENAML_VARIABLE_TYPE_LOCAL, 1, ctx->pma);
    al.arg[1] = xenaml_variable(XENAML_VARIABLE_TYPE_LOCAL, 0, ctx->pma);
    al.count = 2;
    condition = xenaml_logic(XENAML_LOGIC_FUNC_LESSTHAN, &al, ctx->pma);

    /* Now the loop body */
    /* Index (Local2, Local1, 0) */
    al.arg[0] = xenaml_variable(XENAML_VARIABLE_TYPE_LOCAL, 2, ctx->pma);
    al.arg[1] = xenaml_variable(XENAML_VARIABLE_TYPE_LOCAL, 1, ctx->pma);
    al.arg[2] = xenaml_integer(0, XENAML_INT_ZERO, ctx->pma);
    al.count = 3;
    next = xenaml_misc(XENAML_MISC_FUNC_INDEX, &al, ctx->pma);

    /* Store (P98, Index (Local2, Local1)) */
    al.arg[0] = xenaml_name_reference("P98_", NULL, ctx->pma);
    al.arg[1] = next;
    al.count = 2;
    current = xenaml_misc(XENAML_MISC_FUNC_STORE, &al, ctx->pma);
    first = current;

    /* Increment (Local1) */
    al.arg[0] = xenaml_variable(XENAML_VARIABLE_TYPE_LOCAL, 1, ctx->pma);
    al.count = 1;
    next = xenaml_math(XENAML_MATH_FUNC_INCREMENT, &al, ctx->pma);
    wmi_chain_peers(&current, &next);

    loop = xenaml_while(condition, first, ctx->pma);

    /* Build the first part of the method */
    /* Store (108, P96) */
    al.arg[0] = xenaml_integer(XENACPI_WMI_CMD_OUT_BUFFER_SIZE, XENAML_INT_OPTIMIZE, ctx->pma);
    al.arg[1] = xenaml_name_reference("P96_", NULL, ctx->pma);
    al.count = 2;
    current = xenaml_misc(XENAML_MISC_FUNC_STORE, &al, ctx->pma);
    first = current;

    /* Store (P9A, Local0) */
    al.arg[0] = xenaml_name_reference("P9A_", NULL, ctx->pma);
    al.arg[1] = xenaml_variable(XENAML_VARIABLE_TYPE_LOCAL, 0, ctx->pma);
    al.count = 2;
    next = xenaml_misc(XENAML_MISC_FUNC_STORE, &al, ctx->pma);
    wmi_chain_peers(&current, &next);

    /* Store (Buffer (Local0) {}, Local2) */
    binit.init_type = XENAML_BUFFER_INIT_VARLEN;
    binit.aml_buffer.aml_varlen.var_type = XENAML_VARIABLE_TYPE_LOCAL;
    binit.aml_buffer.aml_varlen.var_num = 0;
    al.arg[0] = xenaml_buffer(&binit, ctx->pma);
    al.arg[1] = xenaml_variable(XENAML_VARIABLE_TYPE_LOCAL, 2, ctx->pma);
    al.count = 2;
    next = xenaml_misc(XENAML_MISC_FUNC_STORE, &al, ctx->pma);
    wmi_chain_peers(&current, &next);

    /*  Store (0, Local1) */
    al.arg[0] = xenaml_integer(0, XENAML_INT_ZERO, ctx->pma);
    al.arg[1] = xenaml_variable(XENAML_VARIABLE_TYPE_LOCAL, 1, ctx->pma);
    al.count = 2;
    next = xenaml_misc(XENAML_MISC_FUNC_STORE, &al, ctx->pma);
    wmi_chain_peers(&current, &next);

    /* Store (107, P96) */
    al.arg[0] = xenaml_integer(XENACPI_WMI_CMD_OUT_BUFFER, XENAML_INT_OPTIMIZE, ctx->pma);
    al.arg[1] = xenaml_name_reference("P96_", NULL, ctx->pma);
    al.count = 2;
    next = xenaml_misc(XENAML_MISC_FUNC_STORE, &al, ctx->pma);
    wmi_chain_peers(&current, &next);

    /* Chain the loop on before the return */
    wmi_chain_peers(&current, &loop);

    /* Return (Local2) */
    al.arg[0] = xenaml_variable(XENAML_VARIABLE_TYPE_LOCAL, 2, ctx->pma);
    al.count = 1;
    next = xenaml_misc(XENAML_MISC_FUNC_RETURN, &al, ctx->pma);
    wmi_chain_peers(&current, &next);

    return xenaml_method("OBUF", 0, 1, first, ctx->pma);
}

static void* wif1_device(struct wmi_context *ctx)
{
    void *first = NULL, *current, *next, *tmp;

    /* Name (_HID, EisaId ("PNP0A06")) */
    tmp = xenaml_eisaid("PNP0A06", ctx->pma);
    current = xenaml_name_declaration("_HID", tmp, ctx->pma);
    tmp = NULL;
    first = current;

    /* Name (_UID, 0x01) */
    tmp = xenaml_integer(0x01, XENAML_INT_BYTE, ctx->pma);
    next = xenaml_name_declaration("_UID", tmp, ctx->pma);
    tmp = NULL;
    wmi_chain_peers(&current, &next);

    /* Initialize cmd port and communicate invocation type
     * i.e., method execution or query or set object
     * Method (INIT, 1, Serialized)
     * {
     *     Store (100, P96)
     *     Store (Arg0, P98)
     * }
     */
    next = wif1_store_methods(ctx, "INIT", XENACPI_WMI_CMD_INIT, "P96_", "P98_");
    wmi_chain_peers(&current, &next);

    /* Pass the guid pertaining to the operation.
     * Method (GUID, 2, Serialized)
     * {
     *     Store (101, P96)
     *     Store (0x0, Local0)
     *     Store (Arg0, Local1)
     *     While ( LLess (Local0,16))
     *     {
     *         Add(Local1, Local0, Local2)
     *         Store (DerefOf (Index (Arg1, Local2)), P98)
     *         Increment( Local0 )
     *     }
     * }
     */
    next = wif1_guid_method(ctx);
    wmi_chain_peers(&current, &next);

    /* Set the hint information for the associated object id to locate
     * a specific instance when multiple GUID instances occur in the BIOS.
     * Method (SOID, 1, Serialized)
     * {
     *     Store (110, P96)
     *     Store (Arg0, P98)
     * }
     */
    next = wif1_store_methods(ctx, "SOID", XENACPI_WMI_CMD_OBJID_HINT, "P96_", "P98_");
    wmi_chain_peers(&current, &next);

    /* Set the hint information for the associated wmi id to locate
     * a specific instance when multiple GUID instances occur in the BIOS.
     * Method (SWID, 1, Serialized)
     * {
     *     Store (111, P96)
     *     Store (Arg0, P98)
     * }
     */
    next = wif1_store_methods(ctx, "SWID", XENACPI_WMI_CMD_WMIID_HINT, "P96_", "P98_");
    wmi_chain_peers(&current, &next);

    /* Pass instance # for the associated object pertaining
     * to the invocation.
     * Method (INST, 1, Serialized)
     * {
     *     Store (102, P96)
     *     Store (Arg0, P9A)
     * }
     */
    next = wif1_store_methods(ctx, "INST", XENACPI_WMI_CMD_OBJ_INSTANCE, "P96_", "P9A_");
    wmi_chain_peers(&current, &next);

    /* Pass method id relevant to the method about to be
     * executed.
     * Method (MTID, 1, Serialized)
     * {
     *     Store (103, P96)
     *     Store (Arg0, P9A)
     * }
     */
    next = wif1_store_methods(ctx, "MTID", XENACPI_WMI_CMD_METHOD_ID, "P96_", "P9A_");
    wmi_chain_peers(&current, &next);

    /* Pass input buffer pertaining to the current operation
     * Method (IBUF, 1, Serialized)
     * {
     *     Store (105, P96)
     *     Store (SizeOf(Arg0), Local0)
     *     Store (Local0, P9A)
     *     ToBuffer (Arg0, Local1)
     *     Store (0, Local2)
     *     Store (104, P96)
     *     While ( LLess (Local2,Local0) )
     *     {
     *         Store (DerefOf (Index (Local1, Local2)), P98)
     *         Increment (Local2)
     *     }
     * }
     */
    next = wif1_ibuf_method(ctx);
    wmi_chain_peers(&current, &next);

    /* Now that the input arguments are passed, execute the command
     * Method (EXEC, 0, Serialized)
     * {
     *     Store (106, P96)
     * }
     */
    next = wif1_store_methods(ctx, "EXEC", XENACPI_WMI_CMD_EXECUTE, "P96_", "");
    wmi_chain_peers(&current, &next);

    /* Get the output buffer pertaining to the just executed command
     * Method (OBUF, 0, Serialized)
     * {
     *     Store (108, P96)
     *     Store (P9A, Local0)
     *     Store (Buffer (Local0) {}, Local2)
     *     Store (0, Local1)
     *     Store (107, P96)
     *     While ( LLess (Local1, Local0) )
     *     {
     *         Store (P98, Index (Local2, Local1))
     *         Increment (Local1)
     *     }
     *     Return (Local2)
     * }
     */
    next = wif1_obuf_method(ctx);
    wmi_chain_peers(&current, &next);

    return xenaml_device("WIF1", first, ctx->pma);
}

static void* wif1_init_call(struct wmi_context *ctx, enum XENACPI_WMI_OBJ_INVOCATION_TYPE type)
{
    void *arg0;

    /* \_SB.WMIF.INIT (type) */
    arg0 = xenaml_integer(type, XENAML_INT_OPTIMIZE, ctx->pma);
    return xenaml_name_reference("\\_SB_WIF1INIT", arg0, ctx->pma);
}

static void* wif1_guid_call(struct wmi_context *ctx, uint32_t guid_offset)
{
    void *arg0, *arg1, *tmp;

    /* \_SB.WMIF.GUID (guid_offset, _WDG) */
    arg0 = xenaml_integer(guid_offset, XENAML_INT_OPTIMIZE, ctx->pma);
    arg1 = xenaml_name_reference("_WDG", NULL, ctx->pma);
    tmp = arg0;
    wmi_chain_peers(&tmp, &arg1);
    return xenaml_name_reference("\\_SB_WIF1GUID", arg0, ctx->pma);
}

static void* wif1_soid_call(struct wmi_context *ctx, uint8_t oid_hint)
{
    void *arg0;

    /* \_SB.WMIF.SOID (oid_hint) */
    arg0 = xenaml_integer(oid_hint, XENAML_INT_OPTIMIZE, ctx->pma);
    return xenaml_name_reference("\\_SB_WIF1SOID", arg0, ctx->pma);
}

static void* wif1_swid_call(struct wmi_context *ctx, uint8_t wid_hint)
{
    void *arg0;

    /* \_SB.WMIF.SWID (wid_hint) */
    arg0 = xenaml_integer(wid_hint, XENAML_INT_OPTIMIZE, ctx->pma);
    return xenaml_name_reference("\\_SB_WIF1SWID", arg0, ctx->pma);
}

static void* wif1_inst_call(struct wmi_context *ctx)
{
    void *arg0;

    /* \_SB.WMIF.INST (Arg0) */
    arg0 = xenaml_variable(XENAML_VARIABLE_TYPE_ARG, 0, ctx->pma);
    return xenaml_name_reference("\\_SB_WIF1INST", arg0, ctx->pma);
}

static void* wif1_mtid_call(struct wmi_context *ctx)
{
    void *arg0;

    /* \_SB.WMIF.MTID (Arg1) */
    arg0 = xenaml_variable(XENAML_VARIABLE_TYPE_ARG, 1, ctx->pma);
    return xenaml_name_reference("\\_SB_WIF1MTID", arg0, ctx->pma);
}

static void* wif1_ibuf_call(struct wmi_context *ctx, uint32_t arg_num)
{
    void *arg0;

    /* \_SB.WMIF.IBUF (Arg1) for Set */
    /* \_SB.WMIF.IBUF (Arg2) for Method */
    arg0 = xenaml_variable(XENAML_VARIABLE_TYPE_ARG, arg_num, ctx->pma);
    return xenaml_name_reference("\\_SB_WIF1IBUF", arg0, ctx->pma);
}

static void* wif1_exec_call(struct wmi_context *ctx)
{
    /* \_SB.WMIF.EXEC () */
    return xenaml_name_reference("\\_SB_WIF1EXEC", NULL, ctx->pma);
}

static void* wif1_obuf_call(struct wmi_context *ctx)
{
    struct xenaml_args al;

    memset(&al, 0x0, sizeof (al));

    /* Return (\_SB.WMIF.OBUF ()) */
    al.arg[0] = xenaml_name_reference("\\_SB_WIF1OBUF", NULL, ctx->pma);
    al.count = 1;
    return xenaml_misc(XENAML_MISC_FUNC_RETURN, &al, ctx->pma);
}

static void* wmi__wed_method(struct wmi_context *ctx,
                             struct wmi_device *device)
{
    void *first = NULL, *current, *next;
    struct xenaml_args al;

    memset(&al, 0x0, sizeof (al));

    current = wif1_init_call(ctx, XENACPI_WMI_INV_GET_EVENT_DATA);
    first = current;

    next = wif1_swid_call(ctx, (uint8_t)device->wmiid);
    wmi_chain_peers(&current, &next);

    /* Store (109, P96) */
    al.arg[0] = xenaml_integer(XENACPI_WMI_CMD_EVENT_ID, XENAML_INT_OPTIMIZE, ctx->pma);
    al.arg[1] = xenaml_name_reference("P96_", NULL, ctx->pma);
    al.count = 2;
    next = xenaml_misc(XENAML_MISC_FUNC_STORE, &al, ctx->pma);
    wmi_chain_peers(&current, &next);

    /* Store (Arg0, P98) */
    al.arg[0] = xenaml_variable(XENAML_VARIABLE_TYPE_ARG, 0, ctx->pma);
    al.arg[1] = xenaml_name_reference("P98_", NULL, ctx->pma);
    al.count = 2;
    next = xenaml_misc(XENAML_MISC_FUNC_STORE, &al, ctx->pma);
    wmi_chain_peers(&current, &next);

    next = wif1_exec_call(ctx);
    wmi_chain_peers(&current, &next);

    next = wif1_obuf_call(ctx);
    wmi_chain_peers(&current, &next);

    return xenaml_method("_WED", 1, 1, first, ctx->pma);
}

static void* wmi_wm_method(struct wmi_context *ctx,
                           struct wmi_device *device,
                           const char *name,
                           uint32_t guid_offset)
{
    void *first = NULL, *current, *next;

    current = wif1_init_call(ctx, XENACPI_WMI_INV_EXEC_METHOD);
    first = current;

    next = wif1_guid_call(ctx, guid_offset);
    wmi_chain_peers(&current, &next);

    next = wif1_swid_call(ctx, (uint8_t)device->wmiid);
    wmi_chain_peers(&current, &next);

    next = wif1_inst_call(ctx);
    wmi_chain_peers(&current, &next);

    next = wif1_mtid_call(ctx);
    wmi_chain_peers(&current, &next);

    next = wif1_ibuf_call(ctx, 2);
    wmi_chain_peers(&current, &next);

    next = wif1_exec_call(ctx);
    wmi_chain_peers(&current, &next);

    next = wif1_obuf_call(ctx);
    wmi_chain_peers(&current, &next);

    return xenaml_method(name, 3, 1, first, ctx->pma);
}

static void* wmi_wq_method(struct wmi_context *ctx,
                           struct wmi_device *device,
                           const char *name,
                           uint32_t guid_offset)
{
    void *first = NULL, *current, *next;

    current = wif1_init_call(ctx, XENACPI_WMI_INV_QUERY_OBJECT);
    first = current;

    next = wif1_guid_call(ctx, guid_offset);
    wmi_chain_peers(&current, &next);

    next = wif1_swid_call(ctx, (uint8_t)device->wmiid);
    wmi_chain_peers(&current, &next);

    next = wif1_inst_call(ctx);
    wmi_chain_peers(&current, &next);

    next = wif1_exec_call(ctx);
    wmi_chain_peers(&current, &next);

    next = wif1_obuf_call(ctx);
    wmi_chain_peers(&current, &next);

    return xenaml_method(name, 1, 1, first, ctx->pma);
}

static void* wmi_ws_method(struct wmi_context *ctx,
                           struct wmi_device *device,
                           const char *name,
                           uint32_t guid_offset)
{
    void *first = NULL, *current, *next;

    current = wif1_init_call(ctx, XENACPI_WMI_INV_SET_OBJECT);
    first = current;

    next = wif1_guid_call(ctx, guid_offset);
    wmi_chain_peers(&current, &next);

    next = wif1_swid_call(ctx, (uint8_t)device->wmiid);
    wmi_chain_peers(&current, &next);

    next = wif1_inst_call(ctx);
    wmi_chain_peers(&current, &next);

    next = wif1_ibuf_call(ctx, 1);
    wmi_chain_peers(&current, &next);

    next = wif1_exec_call(ctx);
    wmi_chain_peers(&current, &next);

    return xenaml_method(name, 2, 1, first, ctx->pma);
}

static void* wmi_mof(struct wmi_context *ctx,
                     struct wmi_device *device,
                     const char *name)
{
    struct xenaml_buffer_init buffer_init;
    void *tmp;

    /* Name (WQXX, Buffer (0xNN)) is formed from the raw MOF data. */
    buffer_init.init_type = XENAML_BUFFER_INIT_RAWDATA;
    buffer_init.aml_buffer.aml_rawdata.buffer = device->mofdata;
    buffer_init.aml_buffer.aml_rawdata.raw_length = device->mofsize;
    tmp = xenaml_buffer(&buffer_init, ctx->pma);
    return xenaml_name_declaration(name, tmp, ctx->pma);
}

static void* wmi_create_device(struct wmi_context *ctx, struct wmi_device *device)
{
    void *first = NULL, *current, *next, *tmp;
    struct xenaml_buffer_init buffer_init;
    char name[5] = {0};
    uint32_t luid, i, j;
    char *eptr;

    /* Name (_HID, EisaId ("PNP0C14")) */
    tmp = xenaml_eisaid("PNP0C14", ctx->pma);
    current = xenaml_name_declaration("_HID", tmp, ctx->pma);
    first = current;

    /* Name (_UID, <value-from-fw>) */
    luid = strtol(device->_uid, &eptr, 0);
    if ( luid == 0 && eptr == device->_uid ) {
        /* _UID was a string value */
        tmp = xenaml_string(device->_uid, ctx->pma);
    }
    else {
        /* _UID was a number */
        tmp = xenaml_integer(luid, XENAML_INT_OPTIMIZE, ctx->pma);
    }
    next = xenaml_name_declaration("_UID", tmp, ctx->pma);
    wmi_chain_peers(&current, &next);

    /* Name (_WDG, Buffer (0xNN)) is formed from the GUID data. */
    buffer_init.init_type = XENAML_BUFFER_INIT_RAWDATA;
    buffer_init.aml_buffer.aml_rawdata.buffer = (uint8_t*)device->gblocks;
    buffer_init.aml_buffer.aml_rawdata.raw_length = XENACPI_WMI_GUID_BLOCK_SIZE*(device->gcount);
    tmp = xenaml_buffer(&buffer_init, ctx->pma);
    next = xenaml_name_declaration("_WDG", tmp, ctx->pma);
    wmi_chain_peers(&current, &next);

    next = wmi__wed_method(ctx, device);
    wmi_chain_peers(&current, &next);
    /* Leave the _WED in there as the current method in the chain */

    /* Well I am not sure what a WMI device with no methods really is
     * but it won't hurt anything to make one.
     */
    if ( device->gcount == 0 )
        goto no_methods;

    for ( i = 0, j = 0; i < device->gcount; i++ )
    {
        /* Skip events, they are setup in the _GPE block */
        if ( device->gblocks[i].flags & XENACPI_REGFLAG_EVENT )
            continue;

        /* Need the name chars to make method names */
        name[2] = device->gblocks[i].wmi_type.object_id[0];
        name[3] = device->gblocks[i].wmi_type.object_id[1];

        /* Methods are straitforward - just make one method */
        if ( device->gblocks[i].flags & XENACPI_REGFLAG_METHOD )
        {
            name[0] = 'W';
            name[1] = 'M';
            next = wmi_wm_method(ctx, device, name, i*XENACPI_WMI_GUID_BLOCK_SIZE);
            wmi_chain_peers(&current, &next);
            continue;
        }

        /* The rest are data blocks including the special MOF block which is handled first */
        if ( memcmp(&device->gblocks[i].guid[0], XENACPI_WMI_MOF_GUID, XENACPI_WMI_GUID_SIZE) == 0 )
        {
            name[0] = 'W';
            name[1] = 'Q';
            next = wmi_mof(ctx, device, name);
            wmi_chain_peers(&current, &next);
            continue;
        }

        /* Data blocks are more complicated since it is not known if they are query, set or both.
         * It could be determined by asking ACPI about both but a simpler approach is just to make
         * both. If one is not in the firmware, the MOF will indicate that and it will just never be
         * used.
         */
        name[0] = 'W';
        name[1] = 'Q';
        next = wmi_wq_method(ctx, device, name, i*XENACPI_WMI_GUID_BLOCK_SIZE);
        wmi_chain_peers(&current, &next);

        name[0] = 'W';
        name[1] = 'S';
        next = wmi_ws_method(ctx, device, name, i*XENACPI_WMI_GUID_BLOCK_SIZE);
        wmi_chain_peers(&current, &next);
    }

no_methods:
    return xenaml_device(device->name, first, ctx->pma);
}

static void* wmi_enum_devices(struct wmi_context *ctx,
                              void **last_out)
{
    void *first = NULL, *current, *next;
    uint32_t i;

    *last_out = NULL;

    for ( i = 0; i < ctx->count; i++ )
    {
        if ( ctx->devices[i].skip )
            continue;

        next = wmi_create_device(ctx, &ctx->devices[i]);
        if (first != NULL)
        {
            wmi_chain_peers(&current, &next);
        }
        else
        {
            first = next;
            current = next;
        }
    }

    *last_out = current;

    return first;
}

static void* wmi_gpe_notify(struct wmi_context *ctx, const char *devname, uint32_t wmiid)
{
    void *notify, *condition;
    struct xenaml_args al;
    char fname[3*XENAML_NAME_SIZE] = {0};

    memset(&al, 0x0, sizeof (al));

    strncpy(fname, "\\_SB_", 5);
    strncat(fname, devname, 4);

    /* If ( Equal (Local1, <wmiid>)
     * {
     *     Notify (\_SB.<devname>, Local0)
     * }
     */
    al.arg[0] = xenaml_name_reference(fname, NULL, ctx->pma);
    al.arg[1] = xenaml_variable(XENAML_VARIABLE_TYPE_LOCAL, 0, ctx->pma);
    al.count = 2;
    notify = xenaml_misc(XENAML_MISC_FUNC_NOTIFY, &al, ctx->pma);

    al.arg[0] = xenaml_variable(XENAML_VARIABLE_TYPE_LOCAL, 1, ctx->pma);
    al.arg[1] = xenaml_integer(wmiid, XENAML_INT_OPTIMIZE, ctx->pma);
    al.count = 2;
    condition = xenaml_logic(XENAML_LOGIC_FUNC_EQUAL, &al, ctx->pma);

    return xenaml_if(condition, notify, ctx->pma);
}

static void* wmi_create_gpe_scope(struct wmi_context *ctx)
{
    void *first = NULL, *current, *next;
    struct xenaml_args al;
    uint32_t i;

    memset(&al, 0x0, sizeof (al));

    /* Store (\_SB.P97, Local0) */
    al.arg[0] = xenaml_name_reference("\\_SB_P97_", NULL, ctx->pma);
    al.arg[1] = xenaml_variable(XENAML_VARIABLE_TYPE_LOCAL, 0, ctx->pma);
    al.count = 2;
    current = xenaml_misc(XENAML_MISC_FUNC_STORE, &al, ctx->pma);
    first = current;

    /* Store (\_SB.P99, Local1) */
    al.arg[0] = xenaml_name_reference("\\_SB_P99_", NULL, ctx->pma);
    al.arg[1] = xenaml_variable(XENAML_VARIABLE_TYPE_LOCAL, 1, ctx->pma);
    al.count = 2;
    next = xenaml_misc(XENAML_MISC_FUNC_STORE, &al, ctx->pma);
    wmi_chain_peers(&current, &next);

    for ( i = 0; i < ctx->count; i++ )
    {
        if ( ctx->devices[i].skip )
            continue;

        next = wmi_gpe_notify(ctx, ctx->devices[i].name, (i + 1));
        wmi_chain_peers(&current, &next);
    }

    /* Make the _L18 GPE event handler method: Method (_L18, 0, Serialized) */
    current = xenaml_method("_L18", 0, 1, first, ctx->pma);

    /* Finally make a GPE scope for the SSDT */
    return xenaml_scope("\\_GPE", current, ctx->pma);
}

uint8_t* create_wmi_ssdt(uint32_t *length_out, int *err_out)
{
    struct wmi_context *ctx = NULL;
    void *root = NULL;
    void *first = NULL, *current, *next, *last;
    void *sb = NULL;
    void *gpe = NULL;
    int err = 0, ret;
    uint8_t *buffer = NULL;

    *length_out = 0;
    *err_out = 0;

    ret = wmi_create_context(&ctx);
    if ( ret != 0 )
    {
        *err_out = errno;
        xcpmd_log(LOG_ERR, "%s failed to create WMI context\n", __FUNCTION__);
        return NULL;
    }

    ret = xenaml_create_ssdt("Xen", "HVM", 0, ctx->pma, &root, &err);
    if ( ret != 0 )
    {
        *err_out = errno;
        wmi_destroy_context(ctx);
        xcpmd_log(LOG_ERR, "%s failed to create WMI SSDT DefinitionBlock, err: %d\n",
                  __FUNCTION__, err);
        return NULL;
    }

    /* Build the IO port OpRegions for our WMI interface */
    first = wmi_opregions(ctx, &current);

    /* Build the WMI Interface device */
    next = wif1_device(ctx);

    /* Attach WIF1 to the Operation Region chain */
    wmi_chain_peers(&current, &next);

    /* Build platform specific WMI devices and add them */
    next = wmi_enum_devices(ctx, &last);
    if ( next == NULL )
    {
        /* The one place where all this can fail is if there are no WMI devices, but
           this is not really an error. The platform just may not have these devices.
         */
        wmi_destroy_context(ctx);
        xcpmd_log(LOG_INFO, "%s no WMI devices found in platform ACPI firmware, err: %d\n",
                  __FUNCTION__, err);
        return NULL;
    }

    /* Attach WMI devices to WIF1 */
    wmi_chain_peers(&current, &next);

    /* Create System Bus scope to hold all of it */
    sb = xenaml_scope("\\_SB_", first, ctx->pma);

    /* Create a GPE scope for WMI event notification */
    gpe = wmi_create_gpe_scope(ctx);

    /* Attach _SB and _GPE as peers */
    ret = xenaml_chain_peers(sb, gpe, &err);
    assert((ret == 0)&&(err == 0));

    /* Add scopes to SSDT Definition Block */
    ret = xenaml_chain_children(root, sb, &err);
    assert((ret == 0)&&(err == 0));

    /* Now everything is a child of root, just need to write it all out */
    ret = xenaml_write_ssdt(root, &buffer, length_out, &err);
    if ( ret != 0 )
    {
        *err_out = errno;
        xcpmd_log(LOG_ERR, "%s failed to write WMI SSDT DefinitionBlock, err: %d\n",
                  __FUNCTION__, err);
    }

    /* This will clean up all the device resources and the entire premem
     * pool in the AML library (including all the nodes allocated here).
     */
    wmi_destroy_context(ctx);

    return buffer;
}
