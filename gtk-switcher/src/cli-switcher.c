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
#include "rpc.h"

void dump_vms(GPtrArray *vms)
{
    int i;
    for ( i = 0; i < vms->len; ++i ) {
        const char *vm = g_ptr_array_index( vms, i );
        const char *name = vm_get_name( vm );
        const char *uuid = vm_get_uuid( vm );
        int slot = vm_get_slot( vm );
        printf("%d: %s (%s)\n", slot, name, uuid);
        g_free(name);
        g_free(uuid);
    }
}

const char *vm_with_slot(GPtrArray *vms, int slot)
{
    int i;
    for ( i = 0; i < vms->len; ++i ) {
        const char *vm = g_ptr_array_index( vms, i );
        int slot_ = vm_get_slot( vm );
        if (slot_ == slot) {
            return vm;
        }
    }
    return NULL;
}

void go(void)
{
    for (;;) {
        GPtrArray *vms = list_vms();
        if (vms) {
            dump_vms(vms);
            printf("press key 0-9 to switch\n");
            int slot = (int) (fgetc(stdin) - '0');
            if (slot >= 0 && slot <= 9) {
                const char *vm = vm_with_slot(vms, slot);
                if (!vm) {
                    printf("NO SUCH VM!\n");
                } else {
                    vm_switch(vm);
                }
            }
            g_ptr_array_free( vms, TRUE );
        }
    }
}

int main()
{
    int r = 0;
    openlog("gtk-switcher", 0, LOG_USER);
    info("starting");
    g_type_init();
    r = rpc_init();
    if (r < 0)
        goto out;
    info("initialised rpc");
    go();
out:
    closelog();
    return r;
}
