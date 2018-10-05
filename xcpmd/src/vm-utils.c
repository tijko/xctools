/*
 * vm-utils.c
 *
 * Provides functions to look up and cache VM information.
 *
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

#include "vm-utils.h"
#include "rpcgen/xenmgr_client.h"
#include "rpcgen/xenmgr_vm_client.h"

/**
 * This file contains utility functions for looking up and caching VM
 * information, and supplies some DBus magic that is not provided through
 * rpcgen or libxcdbus.
 */


//Contains a map of the xenstore paths, UUIDs, and names of all VMs returned by
//Xenmgr's list_vms rpc.
//Note that this structure can be free'd and replaced during a search
//operation, so clone any rows you don't want to be free'd out from under you.
struct vm_identifier_table * vm_identifier_table = NULL;


//Function prototype
static void dbus_async_callback_dummy(DBusGProxy *proxy, GError *error, void *user_data);


//Allocates memory!
//Frees the global VM identifier table and replaces it with a new one.
void populate_vm_identifier_table() {

    GPtrArray * vm_list;
    struct vm_identifier_table * old_table;

    old_table = vm_identifier_table;
    com_citrix_xenclient_xenmgr_list_vms_(xcdbus_conn, XENMGR_SERVICE, XENMGR_PATH, &vm_list);
    vm_identifier_table = new_vm_identifier_table(vm_list);

    free_vm_identifier_table(old_table);
}


//Allocates memory!
//Creates a new VM identifier table from a GPtrArray of VMs as retrieved from
//DBus. Does not modify the global vm identifier table.
struct vm_identifier_table * new_vm_identifier_table(GPtrArray * vm_list) {

    struct vm_identifier_table * table;
    char * tmp = NULL;
    char * vm;
    unsigned int i;

    if (vm_list == NULL) {
        return NULL;
    }

    //Alloc the table itself.
    table = (struct vm_identifier_table *)calloc(1, sizeof(struct vm_identifier_table));
    table->num_entries = vm_list->len;
    table->entries = (struct vm_identifier_table_row *)calloc(table->num_entries, sizeof(struct vm_identifier_table_row));

    if ((table == NULL) || (table->entries == NULL)) {
        xcpmd_log(LOG_ERR, "Failed to allocate memory\n");
        free_vm_identifier_table(table);
        return NULL;
    }

    //Create a table row for each entry in the GPtrArray of VM paths.
    for (i = 0; i < table->num_entries; ++i) {

        vm = g_ptr_array_index(vm_list, i);

        //Get the VM name.
        property_get_com_citrix_xenclient_xenmgr_vm_name_(xcdbus_conn, XENMGR_SERVICE, vm, &tmp);
        if(tmp == NULL) {
            xcpmd_log(LOG_ERR, "Error: Couldn't get name of %s.\n", vm);
            goto fail;
        }

        table->entries[i].name = (char *)malloc(strlen(tmp) + 1);
        if (table->entries[i].name == NULL) {
            xcpmd_log(LOG_ERR, "Failed to allocate memory\n");
            goto fail;
        }
        strcpy(table->entries[i].name, tmp);
        free(tmp);
        tmp = NULL;

        //Copy the VM path.
        table->entries[i].path = (char *)malloc(VM_PATH_LEN + 1); //path_len = 40 = 32 path bytes + 4 underscores + "/vm/" (4), and 1 byte for \0
        if (table->entries[i].path == NULL) {
            xcpmd_log(LOG_ERR, "Failed to allocate memory\n");
            goto fail;
        }
        strncpy(table->entries[i].path, vm, VM_PATH_LEN + 1);

        //Extract the VM UUID from the path.
        table->entries[i].uuid = (char *)malloc(VM_PATH_LEN - VM_PATH_UUID_PREFIX_LEN + 1); //path_len = 36 = 32 path bytes + 4 hyphens, and 1 byte for \0
        if (table->entries[i].uuid == NULL) {
            xcpmd_log(LOG_ERR, "Failed to allocate memory\n");
            goto fail;
        }
        strncpy(table->entries[i].uuid, vm+VM_PATH_UUID_PREFIX_LEN, VM_PATH_LEN - VM_PATH_UUID_PREFIX_LEN + 1);

        //Convert _ to - in uuid
        //000000000011111111112222222222333333
        //012345678901234567890123456789012345
        //12345678-1234-1234-1234-123456789012
        table->entries[i].uuid[8] = '-';
        table->entries[i].uuid[13] = '-';
        table->entries[i].uuid[18] = '-';
        table->entries[i].uuid[23] = '-';
    }

    return table;

fail:
    if (tmp) {
        g_free(tmp);
    }

    free_vm_identifier_table(table);
    return NULL;

}


//Allocates memory!
//Search the vmid table for a VM with the given name.
//Returns an alloc'd vmid table row.
//Free result with free_vmid_search_result().
struct vm_identifier_table_row * new_vmid_search_result_by_name(char * name) {

    if (name == NULL)
        return NULL;

    if (vm_identifier_table == NULL) {
        populate_vm_identifier_table();

        if (vm_identifier_table == NULL) {
            xcpmd_log(LOG_WARNING, "Vm identifier table could not be populated. Exiting search.");
            return NULL;
        }
    }

    unsigned int i;

    for (i=0; i < vm_identifier_table->num_entries; ++i) {
        if (0 == strcmp(vm_identifier_table->entries[i].name, name))
            return clone_vmid_table_row(&(vm_identifier_table->entries[i]));
    }

    return NULL;
}


//Allocates memory!
//Search the vmid table for a VM with the given UUID.
//Returns an alloc'd vmid table row.
//Free result with free_vmid_search_result().
struct vm_identifier_table_row * new_vmid_search_result_by_uuid(char * uuid) {

    unsigned int i;

    if (uuid == NULL)
        return NULL;

    if (vm_identifier_table == NULL) {
        populate_vm_identifier_table();

        if (vm_identifier_table == NULL) {
            xcpmd_log(LOG_WARNING, "Vm identifier table could not be populated. Exiting search.");
            return NULL;
        }
    }

    for (i=0; i < vm_identifier_table->num_entries; ++i) {
        if (0 == strcmp(vm_identifier_table->entries[i].uuid, uuid))
            return clone_vmid_table_row(&(vm_identifier_table->entries[i]));
    }

    return NULL;
}


//Allocates memory!
//Search the vmid table for a VM with the given xenstore path.
//Returns an alloc'd vmid table row.
//Free result with free_vmid_search_result().
struct vm_identifier_table_row * new_vmid_search_result_by_path(char * path) {

    unsigned int i;

    if (path == NULL)
        return NULL;

    if (vm_identifier_table == NULL) {
        populate_vm_identifier_table();

        if (vm_identifier_table == NULL) {
            xcpmd_log(LOG_WARNING, "Vm identifier table could not be populated. Exiting search.");
            return NULL;
        }
    }

    for (i=0; i < vm_identifier_table->num_entries; ++i) {
        if (0 == strcmp(vm_identifier_table->entries[i].path, path))
            return clone_vmid_table_row(&(vm_identifier_table->entries[i]));
    }

    return NULL;
}


//Allocates memory!
//Clones a vmid table row and all its members.
struct vm_identifier_table_row * clone_vmid_table_row(struct vm_identifier_table_row * ir) {
    struct vm_identifier_table_row * r;
    char ** irc;
    char ** rc;
    unsigned int i;
    unsigned int l;

    if (!ir) {
        return NULL;
    }

    r = (struct vm_identifier_table_row *)calloc(1, sizeof(struct vm_identifier_table_row));
    if (!r) {
        return NULL;
    }

    irc = (char **)ir;
    rc = (char **)r;
    for (i=0; i < 3; ++i) {
        if (irc[i]) {
            l = strlen(irc[i]);
            if (l > 0) {
                rc[i] = (char *)calloc(l + 1, 1);
                if (!(rc[i])) {
                    free_vmid_search_result(r);
                    return NULL;
                }
                strncpy(rc[i], irc[i], l + 1);
                rc[i][l] = '\0'; //just to be positive
            }
        }
    }
    return r;
}


//Allocates memory!
//Gets the dependencies of a VM (specified by xenstore path), and places the
//results as a GPtrArray in *ary. Returns 0 on failure or 1 on success.
int get_vm_dependencies(const char * vm_path, GPtrArray ** ary) {

    GValue gval;
    int status;

    status = dbus_get_property(xcdbus_conn, XENMGR_SERVICE, vm_path, XENMGR_VM_INTERFACE, "dependencies", &gval);
    *ary = (GPtrArray *)g_value_get_boxed(&gval);

    return status;
}


//Allocates memory!
//Gets the type of a VM. (ndvm, vpnvm, etc)
int get_vm_type(const char * vm_path, char ** type) {

    GValue gval;
    int status;
    
    status = dbus_get_property(xcdbus_conn, XENMGR_SERVICE, vm_path, XENMGR_VM_INTERFACE, "type", &gval);
    *type = strdup((char *)g_value_get_string(&gval));
    xcpmd_log(LOG_DEBUG, "Type of %s is %s.\n", vm_path, *type);
    g_value_unset(&gval);

    return status;
}


//Frees all members of a vmid table row.
void free_vm_identifier_table_row_data(struct vm_identifier_table_row * r) {

    char ** rc;
    unsigned int i;

    if (!r) {
        return;
    }

    rc = (char **)r;
    for (i=0; i < 3; ++i) {
        if (rc[i]) {
            free(rc[i]);
        }
    }
}


//Frees a vmid table.
void free_vm_identifier_table(struct vm_identifier_table * table) {
    if (table == NULL) {
        return;
    }
    if (table->entries != NULL) {
        while (table->num_entries-- > 0) {
            free_vm_identifier_table_row_data(&(table->entries[table->num_entries]));
        }
        free(table->entries);
    }
    free(table);
}


//Frees the result of a vmid search.
void free_vmid_search_result(struct vm_identifier_table_row * r) {
    free_vm_identifier_table_row_data(r);
    free(r);
}


//Retrieves a property over DBus. Returns 0 on failure or 1 on success.
int dbus_get_property(xcdbus_conn_t * xc_conn, const char * service, const char * path, const char * interface, const char * property, GValue * outv) {

    GError *error = NULL;
    GValue var = G_VALUE_INIT;
    DBusGProxy *p = xcdbus_get_proxy(xc_conn, service, path, "org.freedesktop.DBus.Properties");

    if (!p) {
        xcpmd_log(LOG_DEBUG, "Failed to get dbusgproxy");
        return 0;
    }

    if (!dbus_g_proxy_call(p, "Get", &error, G_TYPE_STRING, interface, G_TYPE_STRING, property, G_TYPE_INVALID, G_TYPE_VALUE, &var, G_TYPE_INVALID )) {
        xcpmd_log(LOG_DEBUG, "proxy call failed: %s", error->message);
        return 0;
    }
    *outv = var;
    return 1;

}


//Adds a DBus match for the specified string and registers a filter function,
//with optional argument func_data and optional function free_func that will
//be called on func_data when this match is removed.
//Returns 0 on failure or 1 on success.
//TODO: Cache DBus connection, since libxcdbus doesn't.
int add_dbus_filter(char * match, DBusHandleMessageFunction filter_func, void * func_data, DBusFreeFunction free_func) {

    DBusConnection * connection;

    if (xcdbus_conn == NULL) {
        xcpmd_log(LOG_WARNING, "xcdbus connection is unavailable");
        return 0;
    }

    connection = xcdbus_get_dbus_connection(xcdbus_conn);
    if (connection == NULL) {
        xcpmd_log(LOG_WARNING, "Unable to get DBus connection from xcdbus");
        return 0;
    }

    dbus_bus_add_match(connection, match, NULL);
    if (!dbus_connection_add_filter(connection, filter_func, func_data, free_func)) {
        xcpmd_log(LOG_ERR, "Couldn't add DBus filter--not enough memory");
        return 0;
    }

    return 1;
}


//Removes a DBus filter previously added with add_dbus_filter.
//Returns 0 on failure or 1 on success.
//TODO: Cache DBus connection, since libxcdbus doesn't.
int remove_dbus_filter(char * match, DBusHandleMessageFunction filter_func, void * func_data) {

    DBusConnection * connection;

    //Don't segfault if something else has already torn down the DBus connection.
    if (xcdbus_conn == NULL) {
        //xcpmd_log(LOG_WARNING, "Tried removing filter, but xcdbus connection is unavailable");
        return 0;
    }

    connection = xcdbus_get_dbus_connection(xcdbus_conn);
    if (connection == NULL) {
        xcpmd_log(LOG_WARNING, "Unable to get dbus connection from xcdbus");
        return 0;
    }

    dbus_bus_remove_match(connection, match, NULL);
    dbus_connection_remove_filter(connection, filter_func, func_data);

    return 1;
}


//Makes asynchronous DBus method calls and discards the results.
void dbus_async_call(char * service, char * obj_path, char * interface, DBusGProxyCall* (*call)(), void * userdata) {

    DBusGProxy *proxy = xcdbus_get_proxy(xcdbus_conn, service, obj_path, interface);
    call(proxy, dbus_async_callback_dummy, (gpointer)userdata);
}


//Version of dbus_async_call that takes one arg.
void dbus_async_call_with_arg(char * service, char * obj_path, char * interface, DBusGProxyCall* (*call)(), void * userdata, void * arg) {

    DBusGProxy *proxy = xcdbus_get_proxy(xcdbus_conn, service, obj_path, interface);
    call(proxy, arg, dbus_async_callback_dummy, (gpointer)userdata);
}


//This callback discards any DBus response we may have received.
static void dbus_async_callback_dummy(DBusGProxy *proxy, GError *error, void *user_data) {
    return;
}


//Convenience function to issue a shutdown command and not wait for a response.
void shutdown_vm_async(char * vm_path) {
    dbus_async_call(XENMGR_SERVICE, vm_path, XENMGR_VM_INTERFACE, com_citrix_xenclient_xenmgr_vm_shutdown_async, NULL);
}
