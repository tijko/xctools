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

#ifndef RPC_H_
#define RPC_H_

#include <glib.h>
#include <glib-object.h>
#include <dbus/dbus-glib.h>

int rpc_init(GMainLoop*);
GPtrArray *list_vms(void);
const char *vm_get_uuid(const char* vm);
const char *vm_get_name(const char* vm);
GArray *vm_get_icon_bytes(const char* vm);
int vm_get_slot(const char* vm);
void vm_switch(const char* vm);

void on_vm_changed( void (*cb) (DBusGProxy*, char*, char*, gpointer), gpointer udata );
void on_vm_state_changed( void (*cb)( DBusGProxy*, char*, char*, char*, int, gpointer), gpointer udata );
void on_storage_space_low( void (*cb)( DBusGProxy*, int, gpointer), gpointer udata );
void on_update_state_change( void (*cb)( DBusGProxy*, char*, gpointer), gpointer udata );

#endif
