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

#include "project.h"
#include "xcpmd.h"

#define VM_PATH_LEN             40  // /vm/12345678-1234-1234-1234-123456789012
#define VM_PATH_UUID_PREFIX_LEN 4   // /vm/



//Provides information about a specific VM.
//The order of members in this data structure must be preserved!
struct vm_identifier_table_row {
    char * uuid;
    char * name;
    char * path;
    //Add any new members to the end and remember to update the cloning and
    //freeing functions accordingly!
};


//Contains cached information for a set of VMs.
struct vm_identifier_table {
    unsigned int num_entries;
    struct vm_identifier_table_row * entries;
};


//Global data
extern struct vm_identifier_table * vm_identifier_table;


//Function prototypes
void populate_vm_identifier_table();
struct vm_identifier_table * new_vm_identifier_table(GPtrArray * vm_list);
struct vm_identifier_table_row * new_vmid_search_result_by_name(char * name);
struct vm_identifier_table_row * new_vmid_search_result_by_uuid(char * uuid);
struct vm_identifier_table_row * new_vmid_search_result_by_path(char * path);
struct vm_identifier_table_row * clone_vmid_table_row(struct vm_identifier_table_row * ir);
int get_vm_dependencies(const char * vm_path, GPtrArray ** ary);

void free_vm_identifier_table_row_data(struct vm_identifier_table_row * r);
void free_vm_identifier_table(struct vm_identifier_table * table);
void free_vmid_search_result(struct vm_identifier_table_row * r);

int dbus_get_property(xcdbus_conn_t * xc_conn, const char * service, const char * path, const char * interface, const char * property, GValue * outv);
int add_dbus_filter(char * match, DBusHandleMessageFunction filter_func, void * func_data, DBusFreeFunction free_func);
int remove_dbus_filter(char * match, DBusHandleMessageFunction filter_func, void * func_data);
void dbus_async_call(char * service, char * obj_path, char * interface, DBusGProxyCall* (*call)(), void * userdata);
void dbus_async_call_with_arg(char * service, char * obj_path, char * interface, DBusGProxyCall* (*call)(), void * userdata, void * arg);

void shutdown_vm_async(char * vm_path);
