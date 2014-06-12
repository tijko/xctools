/*
 * wmi.c
 *
 * XEN ACPI WMI access.
 *
 * Copyright (c) 2012 Ross Philipson <ross.philipson@citrix.com>
 * Copyright (c) 2012 Citrix Systems, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include "project.h"
#include <sys/ioctl.h>

/* TODO fix ioctl codes and use standard error codes between driver and lib. Merge drivers too */

#define WMI_DEFAULT_BUFFER 1024

static int
wmi_ioctl(int fd, int request, void *ioctl_buf, xen_wmi_buffer_t *wmi_buf, int *error_out)
{
    int rio;

    if ( wmi_buf != NULL )
    {
        wmi_buf->copied_length = malloc(sizeof(size_t));
        if ( wmi_buf->copied_length == NULL )
            return xenacpi_error(error_out, ENOMEM);

        wmi_buf->pointer = malloc(WMI_DEFAULT_BUFFER);
        if ( wmi_buf->pointer == NULL )
        {
            free(wmi_buf->copied_length);
            return xenacpi_error(error_out, ENOMEM);
        }

        memset(wmi_buf->copied_length, 0, sizeof(size_t));
        memset(wmi_buf->pointer, 0, WMI_DEFAULT_BUFFER);
        wmi_buf->length = WMI_DEFAULT_BUFFER;
    }

    rio = ioctl(fd, request, ioctl_buf);
    if ( rio == -1 && errno == -XEN_WMI_BUFFER_TOO_SMALL && wmi_buf != NULL )
    {
        /* sanity check, expecting something here */
        if ( *wmi_buf->copied_length == 0 )
            return xenacpi_error(error_out, EFAULT);

        /* realloc the buffer */
        free(wmi_buf->pointer);
        wmi_buf->pointer = malloc(*wmi_buf->copied_length);
        if ( wmi_buf->pointer == NULL )
        {
            free(wmi_buf->copied_length);
            return xenacpi_error(error_out, ENOMEM);
        }
        memset(wmi_buf->pointer, 0, *wmi_buf->copied_length);
        wmi_buf->length = *wmi_buf->copied_length;

        rio = ioctl(fd, request, ioctl_buf);
        if ( rio == -1 )
            return xenacpi_error(error_out, errno);
    }
    else if ( rio == -1 )
        return xenacpi_error(error_out, errno);

    return 0;
}

EXTERNAL int
xenacpi_wmi_get_devices(struct xenacpi_wmi_device **devices_out,
                        uint32_t *count_out,
                        int *error_out)
{
    int devfd, ret;
    uint32_t i, count;
    xen_wmi_device_data_t device_data = {0};
    xen_wmi_device_t *wdevices;
    char dev_name[64];

    if ( devices_out == NULL || count_out == NULL )
        return xenacpi_error(error_out, EINVAL);

    sprintf(dev_name, "/dev/%s", XEN_WMI_DEVICE_NAME);
    devfd = open(dev_name, 0);
    if ( devfd < 0 )
        return xenacpi_error(error_out, ENODEV);

    ret = wmi_ioctl(devfd, XEN_WMI_IOCTL_GET_DEVICES, &device_data, &device_data.out_buf, error_out);
    close(devfd);
    if ( ret == -1 )
        return -1;

    wdevices = (xen_wmi_device_t*)device_data.out_buf.pointer;
    count = *device_data.out_buf.copied_length/sizeof(xen_wmi_device_t);

    *devices_out = NULL;
    *count_out = 0;
    ret = 0;

    while ( count > 0 )
    {
        *devices_out = (struct xenacpi_wmi_device*)malloc(count*sizeof(struct xenacpi_wmi_device));
        if ( *devices_out == NULL )
        {
            ret = -1;
            break;
        }

        /* copy these one at a time in case the struct changes internally */
        for ( i = 0; i < count; i++)
        {
            (*devices_out)[i].name = wdevices[i].name;
            (*devices_out)[i].wmiid = wdevices[i].wmiid;
            memcpy(&(*devices_out)[i]._hid[0], &wdevices[i]._hid[0], XENACPI_WMI_NAME_SIZE);
            memcpy(&(*devices_out)[i]._uid[0], &wdevices[i]._uid[0], XENACPI_WMI_NAME_SIZE);
        }
        *count_out = count;

        break;
    }

    free(device_data.out_buf.pointer);
    free(device_data.out_buf.copied_length);

    if ( ret == -1 )
        return xenacpi_error(error_out, ENOMEM);

    return ret;
}

EXTERNAL int
xenacpi_wmi_get_device_blocks(uint32_t wmiid,
                              struct xenacpi_wmi_guid_block **blocks_out,
                              uint32_t *count_out,
                              int *error_out)
{
    int devfd, ret, err;
    uint32_t count;
    xen_wmi_device_block_data_t gblock_data = {0};
    char dev_name[64];

    if ( blocks_out == NULL || count_out == NULL )
        return xenacpi_error(error_out, EINVAL);

    sprintf(dev_name, "/dev/%s", XEN_WMI_DEVICE_NAME);
    devfd = open(dev_name, 0);
    if ( devfd < 0 )
        return xenacpi_error(error_out, ENODEV);

    gblock_data.wmiid = wmiid;
    ret = wmi_ioctl(devfd, XEN_WMI_IOCTL_GET_DEVICE_BLOCKS, &gblock_data, &gblock_data.out_buf, error_out);
    close(devfd);
    if ( ret == -1 )
        return -1;

    count = *gblock_data.out_buf.copied_length/sizeof(xen_wmi_guid_block_t);

    if ( count > 0 )
    {
        /* The guid structure is based on the WMI spec and is not subject to change */
        *blocks_out = (struct xenacpi_wmi_guid_block*)gblock_data.out_buf.pointer;
        *count_out = count;
    }
    else
    {
        free(gblock_data.out_buf.pointer);
        *blocks_out = NULL;
        *count_out = 0;
    }

    free(gblock_data.out_buf.copied_length);

    return 0;
}

EXTERNAL int
xenacpi_wmi_invoke_method(struct xenacpi_wmi_invocation_data *inv_block,
                          void *buffer_in,
                          uint32_t length_in,
                          void **buffer_out,
                          uint32_t *length_out,
                          int *error_out)
{
    int devfd, ret, request;
    xen_wmi_obj_invocation_data_t invocation_data = {0};
    char dev_name[64];

    if ( inv_block == NULL || buffer_out == NULL || length_out == NULL )
        return xenacpi_error(error_out, EINVAL);

    sprintf(dev_name, "/dev/%s", XEN_WMI_DEVICE_NAME);
    devfd = open(dev_name, 0);
    if ( devfd < 0 )
        return xenacpi_error(error_out, ENODEV);

    memcpy(&invocation_data.guid[0], &inv_block->guid[0], XENACPI_WMI_GUID_SIZE);
    if ( inv_block->flags & XENACPI_WMI_FLAG_USE_WMIID )
    {
        request = XEN_WMI_IOCTL_CALL_METHOD_WMIID;
        invocation_data.wmiid = inv_block->wmiid;
    }
    else if ( inv_block->flags & XENACPI_WMI_FLAG_USE_OBJID )
    {
        request = XEN_WMI_IOCTL_CALL_METHOD_OBJID;
        invocation_data.objid[0] = inv_block->objid[0];
        invocation_data.objid[1] = inv_block->objid[1];
    }
    else
        request = XEN_WMI_IOCTL_CALL_METHOD;

    invocation_data.xen_wmi_arg.xen_wmi_method_arg.instance = inv_block->instance;
    invocation_data.xen_wmi_arg.xen_wmi_method_arg.method_id = inv_block->method_id;
    if ( buffer_in != NULL && length_in > 0 )
    {
        invocation_data.xen_wmi_arg.xen_wmi_method_arg.in_buf.length = length_in;
        invocation_data.xen_wmi_arg.xen_wmi_method_arg.in_buf.pointer = buffer_in;
    }

    ret = wmi_ioctl(devfd, request, &invocation_data, &invocation_data.xen_wmi_arg.xen_wmi_method_arg.out_buf, error_out);
    close(devfd);
    if ( ret == -1 )
        return -1;

    if ( *invocation_data.xen_wmi_arg.xen_wmi_method_arg.out_buf.copied_length > 0 )
    {
        *buffer_out = invocation_data.xen_wmi_arg.xen_wmi_method_arg.out_buf.pointer;
        *length_out = *invocation_data.xen_wmi_arg.xen_wmi_method_arg.out_buf.copied_length;
    }
    else
    {
        free(invocation_data.xen_wmi_arg.xen_wmi_method_arg.out_buf.pointer);
        *buffer_out = NULL;
        *length_out = 0;
    }

    free(invocation_data.xen_wmi_arg.xen_wmi_method_arg.out_buf.copied_length);

    return 0;
}

EXTERNAL int
xenacpi_wmi_query_object(struct xenacpi_wmi_invocation_data *inv_block,
                         void **buffer_out,
                         uint32_t *length_out,
                         int *error_out)
{
    int devfd, ret, request;
    xen_wmi_obj_invocation_data_t invocation_data = {0};
    char dev_name[64];

    if ( inv_block == NULL || buffer_out == NULL || length_out == NULL )
        return xenacpi_error(error_out, EINVAL);

    sprintf(dev_name, "/dev/%s", XEN_WMI_DEVICE_NAME);
    devfd = open(dev_name, 0);
    if ( devfd < 0 )
        return xenacpi_error(error_out, ENODEV);

    memcpy(&invocation_data.guid[0], &inv_block->guid[0], XENACPI_WMI_GUID_SIZE);
    if ( inv_block->flags & XENACPI_WMI_FLAG_USE_WMIID )
    {
        request = XEN_WMI_IOCTL_QUERY_OBJECT_WMIID;
        invocation_data.wmiid = inv_block->wmiid;
    }
    else if ( inv_block->flags & XENACPI_WMI_FLAG_USE_OBJID )
    {
        request = XEN_WMI_IOCTL_QUERY_OBJECT_OBJID;
        invocation_data.objid[0] = inv_block->objid[0];
        invocation_data.objid[1] = inv_block->objid[1];
    }
    else
        request = XEN_WMI_IOCTL_QUERY_OBJECT;

    invocation_data.xen_wmi_arg.xen_wmi_query_obj_arg.instance = inv_block->instance;

    ret = wmi_ioctl(devfd, request, &invocation_data, &invocation_data.xen_wmi_arg.xen_wmi_query_obj_arg.out_buf, error_out);
    close(devfd);
    if ( ret == -1 )
        return -1;

    if ( *invocation_data.xen_wmi_arg.xen_wmi_query_obj_arg.out_buf.copied_length > 0 )
    {
        *buffer_out = invocation_data.xen_wmi_arg.xen_wmi_query_obj_arg.out_buf.pointer;
        *length_out = *invocation_data.xen_wmi_arg.xen_wmi_query_obj_arg.out_buf.copied_length;
    }
    else
    {
        free(invocation_data.xen_wmi_arg.xen_wmi_query_obj_arg.out_buf.pointer);
        *buffer_out = NULL;
        *length_out = 0;
    }

    free(invocation_data.xen_wmi_arg.xen_wmi_query_obj_arg.out_buf.copied_length);

    return 0;
}

EXTERNAL int
xenacpi_wmi_set_object(struct xenacpi_wmi_invocation_data *inv_block,
                       void *buffer_in,
                       uint32_t length_in,
                       int *error_out)
{
    int devfd, ret, request;
    xen_wmi_obj_invocation_data_t invocation_data = {0};
    char dev_name[64];

    if ( inv_block == NULL || buffer_in == NULL || length_in == 0 )
        return xenacpi_error(error_out, EINVAL);

    sprintf(dev_name, "/dev/%s", XEN_WMI_DEVICE_NAME);
    devfd = open(dev_name, 0);
    if ( devfd < 0 )
        return xenacpi_error(error_out, ENODEV);

    memcpy(&invocation_data.guid[0], &inv_block->guid[0], XENACPI_WMI_GUID_SIZE);
    if ( inv_block->flags & XENACPI_WMI_FLAG_USE_WMIID )
    {
        request = XEN_WMI_IOCTL_SET_OBJECT_WMIID;
        invocation_data.wmiid = inv_block->wmiid;
    }
    else if ( inv_block->flags & XENACPI_WMI_FLAG_USE_OBJID )
    {
        request = XEN_WMI_IOCTL_SET_OBJECT_OBJID;
        invocation_data.objid[0] = inv_block->objid[0];
        invocation_data.objid[1] = inv_block->objid[1];
    }
    else
        request = XEN_WMI_IOCTL_SET_OBJECT;

    invocation_data.xen_wmi_arg.xen_wmi_set_obj_arg.instance = inv_block->instance;
    invocation_data.xen_wmi_arg.xen_wmi_set_obj_arg.in_buf.length = length_in;
    invocation_data.xen_wmi_arg.xen_wmi_set_obj_arg.in_buf.pointer = buffer_in;

    ret = wmi_ioctl(devfd, request, &invocation_data, NULL, error_out);
    close(devfd);
    if ( ret == -1 )
        return -1;

    return 0;
}

EXTERNAL int xenacpi_wmi_get_event_data(struct xenacpi_wmi_invocation_data *inv_block,
                                        void **buffer_out,
                                        uint32_t *length_out,
                                        int *error_out)
{
    int devfd, ret, request;
    xen_wmi_obj_invocation_data_t invocation_data = {0};
    char dev_name[64];

    if ( inv_block == NULL || buffer_out == NULL || length_out == NULL )
        return xenacpi_error(error_out, EINVAL);

    sprintf(dev_name, "/dev/%s", XEN_WMI_DEVICE_NAME);
    devfd = open(dev_name, 0);
    if ( devfd < 0 )
        return xenacpi_error(error_out, ENODEV);

    memcpy(&invocation_data.guid[0], &inv_block->guid[0], XENACPI_WMI_GUID_SIZE);
    if ( inv_block->flags & XENACPI_WMI_FLAG_USE_WMIID )
    {
        request = XEN_WMI_IOCTL_GET_EVENT_DATA_WMIID;
        invocation_data.wmiid = inv_block->wmiid;
    }
    else
        request = XEN_WMI_IOCTL_GET_EVENT_DATA;

    invocation_data.xen_wmi_arg.xen_wmi_event_data_arg.event_id = inv_block->event_id;

    ret = wmi_ioctl(devfd, request, &invocation_data, &invocation_data.xen_wmi_arg.xen_wmi_event_data_arg.out_buf, error_out);
    close(devfd);
    if ( ret == -1 )
        return -1;

    if ( *invocation_data.xen_wmi_arg.xen_wmi_event_data_arg.out_buf.copied_length > 0 )
    {
        *buffer_out = invocation_data.xen_wmi_arg.xen_wmi_event_data_arg.out_buf.pointer;
        *length_out = *invocation_data.xen_wmi_arg.xen_wmi_event_data_arg.out_buf.copied_length;
    }
    else
    {
        free(invocation_data.xen_wmi_arg.xen_wmi_event_data_arg.out_buf.pointer);
        *buffer_out = NULL;
        *length_out = 0;
    }

    free(invocation_data.xen_wmi_arg.xen_wmi_event_data_arg.out_buf.copied_length);

    return 0;
}
