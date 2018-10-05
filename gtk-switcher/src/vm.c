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

#include <gtk/gtk.h>
#include <dbus/dbus-glib.h>
#include <xs.h>
#include <string.h>

#define VM_STATE_ASLEEP 3
#define VM_STATE_RUNNING "running"

char* guestPath = "";

int
xenstore_init()
{
	return get_guest_path();  
}

int
get_guest_path()
{
	struct xs_handle* xs;
    unsigned int len = 0;
    int i;

	xs = xs_domain_open();
    guestPath = xs_read(xs, XBT_NULL, "vm", &len);
	xs_close(xs);
	
	if (len == 0)
	{
		return -1;
	}
	
	for (i = 0; i < strlen(guestPath); i++)
	{
		if (guestPath[i] == '-')
		{
			guestPath[i] = '_';
		}
	}
	
	return 0;
}

int
vm_is_guest(char* vm)
{
	return !strcmp(vm, guestPath);
}

int
vm_is_visible(char* vm)
{
	// visible if running, asleep and hidden_in_switcher = false
	if (vm_get_hidden_in_switcher(vm))
	{
		return FALSE;
	}
	
	if (vm_get_acpi_state(vm) == VM_STATE_ASLEEP)
	{
		return TRUE;
	}
	
	return !strcmp( (char*) vm_get_state(vm), VM_STATE_RUNNING);
}

void
dump_vms(GPtrArray *vms)
{
    int i;
    for ( i = 0; i < vms->len; ++i ) {
        const char *vm = g_ptr_array_index( vms, i );
        const char *name = (char*) vm_get_name( vm );
        const char *uuid = (char*) vm_get_uuid( vm );
        int slot = vm_get_slot( vm );
        printf("%d: %s (%s)\n", slot, name, uuid);
        g_free((char*)name);
        g_free((char*)uuid);
    }
}

/* create image widget in a really LAME way */
GdkPixbuf*
get_icon( GArray *a )
{
    FILE *f = fopen( "/tmp/lol.png", "wb" );
    char *p = a->data;
    int bytes = a->len;
    if (f) {
        while (bytes > 0) {
            int w = fwrite( p, 1, (4096 >= bytes) ? 4096 : bytes, f );
            if (w < 0) {
                return NULL;
            }
            p += w;
            bytes -= w;
        }
        fclose(f);
    }
    return gdk_pixbuf_new_from_file( "/tmp/lol.png", NULL );
}
