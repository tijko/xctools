/*
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

#include "project.h"
#include <xcdbus.h>
#include "rpcgen/xenmgr_client.h"
#include "rpcgen/xenmgr_vm_client.h"
#include "marshall.h"

static xcdbus_conn_t *xcbus = NULL;

static DBusGProxy *xenmgr_proxy = NULL;
static DBusGProxy *xenmgr_host_proxy = NULL;
static DBusGProxy *update_proxy = NULL;

#define XENMGR "com.citrix.xenclient.xenmgr"
#define XENMGR_OBJ "/"
#define XENMGR_HOST_OBJ "/host"

#define UPDATEMGR "com.citrix.xenclient.updatemgr"
#define UPDATEMGR_OBJ "/"

/*
com.citrix.xenclient.xenmgr
---------------------------------
vm_created
vm_deleted
vm_state_changed
vm_config_changed
	<refresh>

com.citrix.xenclient.xenmgr.host
---------------------------------
storage_space_low
	Disk space low
	Only {xx}% of your disk space is currently free.

com.citrix.xenclient.updatemgr
---------------------------------
update_state_change (state = 'downloaded-files'):
 	XenClient update is ready.
	To apply this update, you must restart your computer
*/

int rpc_init(GMainLoop *loop)
{
    xcbus = xcdbus_init_with_gloop( NULL, NULL, loop );
    if (!xcbus)
        return -1;

    xenmgr_proxy = xcdbus_get_proxy( xcbus, XENMGR, XENMGR_OBJ, "com.citrix.xenclient.xenmgr" );
    xenmgr_host_proxy = xcdbus_get_proxy( xcbus, XENMGR, XENMGR_HOST_OBJ, "com.citrix.xenclient.xenmgr.host" );
    update_proxy = xcdbus_get_proxy( xcbus, UPDATEMGR, UPDATEMGR_OBJ, "com.citrix.xenclient.updatemgr" );

    dbus_g_object_register_marshaller(
        g_cclosure_user_marshal_VOID__INT,
        G_TYPE_NONE, G_TYPE_INT, G_TYPE_INVALID );
	
	dbus_g_object_register_marshaller(
        g_cclosure_user_marshal_VOID__STRING,
        G_TYPE_NONE, G_TYPE_STRING, G_TYPE_INVALID );
        
	dbus_g_object_register_marshaller(
        g_cclosure_user_marshal_VOID__STRING_STRING,
        G_TYPE_NONE, G_TYPE_STRING, DBUS_TYPE_G_OBJECT_PATH, G_TYPE_INVALID );
    
    dbus_g_object_register_marshaller(
        g_cclosure_user_marshal_VOID__STRING_STRING_STRING_INT,
        G_TYPE_NONE, G_TYPE_STRING, DBUS_TYPE_G_OBJECT_PATH, G_TYPE_STRING, G_TYPE_INT, G_TYPE_INVALID );

    dbus_g_proxy_add_signal( xenmgr_proxy, "vm_created", G_TYPE_STRING, DBUS_TYPE_G_OBJECT_PATH, G_TYPE_INVALID );
    dbus_g_proxy_add_signal( xenmgr_proxy, "vm_deleted", G_TYPE_STRING, DBUS_TYPE_G_OBJECT_PATH, G_TYPE_INVALID );
    dbus_g_proxy_add_signal( xenmgr_proxy, "vm_config_changed", G_TYPE_STRING, DBUS_TYPE_G_OBJECT_PATH, G_TYPE_INVALID );
    dbus_g_proxy_add_signal( xenmgr_proxy, "vm_state_changed", G_TYPE_STRING, DBUS_TYPE_G_OBJECT_PATH, G_TYPE_STRING, G_TYPE_INT, G_TYPE_INVALID );

    dbus_g_proxy_add_signal( xenmgr_host_proxy, "storage_space_low", G_TYPE_INT, G_TYPE_INVALID );    
   
    dbus_g_proxy_add_signal( update_proxy, "update_state_change", G_TYPE_STRING, G_TYPE_INVALID );

    return 0;
}

GPtrArray* list_vms(void)
{
    GPtrArray *paths;
    int i;
    if (!com_citrix_xenclient_xenmgr_list_vms_( xcbus, XENMGR, XENMGR_OBJ, &paths)) {
        warning("listing vms failed");
        return NULL;
    }
    return paths;
}

const char* vm_get_uuid( const char *vm )
{
    char *val = NULL;
    if (!property_get_com_citrix_xenclient_xenmgr_vm_uuid_ ( xcbus, XENMGR, vm, &val )) {
        warning("property access failed");
    }
    return val;
}

const char* vm_get_name( const char *vm )
{
    char *val = NULL;
    if (!property_get_com_citrix_xenclient_xenmgr_vm_name_ ( xcbus, XENMGR, vm, &val )) {
        warning("property access failed");
    }
    return val;
}

const char* vm_get_state( const char *vm )
{
    char *val = NULL;
    if (!property_get_com_citrix_xenclient_xenmgr_vm_state_ ( xcbus, XENMGR, vm, &val )) {
        warning("property access failed");
    }
    return val;
}

int vm_get_acpi_state( const char *vm )
{
    int val = -1;
    if (!property_get_com_citrix_xenclient_xenmgr_vm_acpi_state_ ( xcbus, XENMGR, vm, &val )) {
        warning("property access failed");
    }
    return val;
}

int vm_get_hidden_in_switcher( const char *vm )
{
    int val = -1;
    if (!property_get_com_citrix_xenclient_xenmgr_vm_hidden_in_switcher_ ( xcbus, XENMGR, vm, &val )) {
        warning("property access failed");
    }
    return val;
}

GArray* vm_get_icon_bytes( const char *vm )
{
	printf("vm_get_icon_bytes for %s\n", vm);
    GArray *arr = NULL;
    if (!com_citrix_xenclient_xenmgr_vm_read_icon_ ( xcbus, XENMGR, vm, &arr )) {
        warning("icon read failed");
    }
    return arr;
}

int vm_get_slot( const char *vm )
{
    int val = -1;
    if (!property_get_com_citrix_xenclient_xenmgr_vm_slot_ ( xcbus, XENMGR, vm, &val )) {
        warning("property access failed");
    }
    return val;
}

void vm_switch( const char *vm )
{
    if (!com_citrix_xenclient_xenmgr_vm_switch_( xcbus, XENMGR, vm )) {
        warning("switch failed");
    }
}

void on_vm_changed( void (*cb)( DBusGProxy*, char*, char*, gpointer), gpointer udata )
{
    dbus_g_proxy_connect_signal( xenmgr_proxy, "vm_created", G_CALLBACK( cb ), NULL, NULL);
    dbus_g_proxy_connect_signal( xenmgr_proxy, "vm_deleted", G_CALLBACK( cb ), NULL, NULL);
    dbus_g_proxy_connect_signal( xenmgr_proxy, "vm_config_changed", G_CALLBACK( cb ), NULL, NULL);
}

void on_vm_state_changed( void (*cb)( DBusGProxy*, char*, char*, char*, int, gpointer), gpointer udata )
{
    dbus_g_proxy_connect_signal( xenmgr_proxy, "vm_state_changed", G_CALLBACK( cb ), NULL, NULL);
}

void on_storage_space_low( void (*cb)( DBusGProxy*, int, gpointer), gpointer udata )
{
    dbus_g_proxy_connect_signal( xenmgr_host_proxy, "storage_space_low", G_CALLBACK( cb ), NULL, NULL);
}

void on_update_state_change( void (*cb)( DBusGProxy*, char*, gpointer), gpointer udata )
{
    dbus_g_proxy_connect_signal( update_proxy, "update_state_change", G_CALLBACK( cb ), NULL, NULL);
}
