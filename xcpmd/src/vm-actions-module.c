/*
 * vm-actions-module.c
 *
 * XCPMD module providing actions affecting VM state.
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

#include <glib.h>
#include <string.h>
#include <stdlib.h>
#include "rpcgen/xenmgr_client.h"
#include "rpcgen/xenmgr_vm_client.h"
#include "rules.h"
#include "project.h"
#include "xcpmd.h"
#include "vm-utils.h"

/*
 * Sink module containing actions affecting VMs. Whenever possible, asynchronous
 * DBus calls are used to reduce latency.
 */

//Function prototypes
void sleep_vm                    (struct arg_node *);
void sleep_vm_by_uuid            (struct arg_node *);
void resume_vm                   (struct arg_node *);
void resume_vm_by_uuid           (struct arg_node *);
void pause_vm                    (struct arg_node *);
void pause_vm_by_uuid            (struct arg_node *);
void unpause_vm                  (struct arg_node *);
void unpause_vm_by_uuid          (struct arg_node *);
void reboot_vm                   (struct arg_node *);
void reboot_vm_by_uuid           (struct arg_node *);
void shutdown_vm                 (struct arg_node *);
void shutdown_vm_by_uuid         (struct arg_node *);
void start_vm                    (struct arg_node *);
void start_vm_by_uuid            (struct arg_node *);
void suspend_vm_to_file          (struct arg_node *);
void suspend_vm_by_uuid_to_file  (struct arg_node *);
void resume_vm_from_file         (struct arg_node *);
void resume_vm_by_uuid_from_file (struct arg_node *);
void shutdown_dependencies_of_vm_by_name (struct arg_node *);
void shutdown_dependencies_of_vm_by_uuid (struct arg_node *);
void shutdown_vpnvm_dependencies_of_vm_by_name (struct arg_node *);
void shutdown_vpnvm_dependencies_of_vm_by_uuid (struct arg_node *);


//Private data structures
struct action_table_row {
    char * name;
    void (* func)(struct arg_node *);
    char * prototype;
    char * pretty_prototype;
};

struct vm_list {
    struct list_head list;
    char * vm_path;
};

struct vm_deps {
    struct list_head list;
    char * vm_path;
    char * vm_state;
    struct vm_identifier_table * deps;
};

//Private data
static struct action_table_row action_table[] = {
    {"sleepVm"                    , sleep_vm                                     , "s"      , "string vm_name"                  },
    {"resumeVm"                   , resume_vm                                    , "s"      , "string vm_name"                  },
    {"pauseVm"                    , pause_vm                                     , "s"      , "string vm_name"                  },
    {"unpauseVm"                  , unpause_vm                                   , "s"      , "string vm_name"                  },
    {"rebootVm"                   , reboot_vm                                    , "s"      , "string vm_name"                  },
    {"shutdownVm"                 , shutdown_vm                                  , "s"      , "string vm_name"                  },
    {"startVm"                    , start_vm                                     , "s"      , "string vm_name"                  },
    {"suspendVmToFile"            , suspend_vm_to_file                           , "s s"    , "string vm_name, string filename" },
    {"resumeVmFromFile"           , resume_vm_from_file                          , "s s"    , "string vm_name, string filename" },
    {"sleepVmUuid"                , sleep_vm_by_uuid                             , "s"      , "string vm_uuid"                  },
    {"resumeVmUuid"               , resume_vm_by_uuid                            , "s"      , "string vm_uuid"                  },
    {"pauseVmUuid"                , pause_vm_by_uuid                             , "s"      , "string vm_uuid"                  },
    {"unpauseVmUuid"              , unpause_vm_by_uuid                           , "s"      , "string vm_uuid"                  },
    {"rebootVmUuid"               , reboot_vm_by_uuid                            , "s"      , "string vm_uuid"                  },
    {"shutdownVmUuid"             , shutdown_vm_by_uuid                          , "s"      , "string vm_uuid"                  },
    {"startVmUuid"                , start_vm_by_uuid                             , "s"      , "string vm_uuid"                  },
    {"suspendVmUuidToFile"        , suspend_vm_by_uuid_to_file                   , "s s"    , "string vm_uuid, string filename" },
    {"resumeVmUuidFromFile"       , resume_vm_by_uuid_from_file                  , "s s"    , "string vm_uuid, string filename" },
    {"shutdownDepsOfVm"           , shutdown_dependencies_of_vm_by_name          , "s"      , "string vm_name"                  },
    {"shutdownDepsOfVmUuid"       , shutdown_dependencies_of_vm_by_uuid          , "s"      , "string vm_uuid"                  },
    {"shutdownVpnvmsForVm"        , shutdown_vpnvm_dependencies_of_vm_by_name    , "s"      , "string vm_name"                  },
    {"shutdownVpnvmsForVmUuid"    , shutdown_vpnvm_dependencies_of_vm_by_uuid    , "s"      , "string vm_uuid"                  }
};

static unsigned int num_action_types = sizeof(action_table) / sizeof(action_table[0]);
static int times_loaded = 0;


//Registers this module's action types.
//The constructor attribute causes this function to run at load (dlopen()) time.
__attribute__ ((constructor)) static void init_module() {

    unsigned int i;

    for (i=0; i < num_action_types; ++i) {
        add_action_type(action_table[i].name, action_table[i].func, action_table[i].prototype, action_table[i].pretty_prototype);
    }
}


//Cleans up after this module.
//The destructor attribute causes this to run at unload (dlclose()) time.
__attribute__ ((destructor)) static void uninit_module() {
    return;
}


//Actions
void sleep_vm(struct arg_node * args) {

    struct arg_node * node = get_arg(args, 0);
    struct vm_identifier_table_row * vmid = new_vmid_search_result_by_name(node->arg.str);

    if ((!vmid) || (!(vmid->path))) {
        xcpmd_log(LOG_WARNING, "Failed to sleep vm %s--couldn't get UUID\n", node->arg.str);
        free_vmid_search_result(vmid);
        return;
    }

    dbus_async_call("com.citrix.xenclient.xenmgr", vmid->path, "com.citrix.xenclient.xenmgr.vm", com_citrix_xenclient_xenmgr_vm_sleep_async, NULL);
    free_vmid_search_result(vmid);
}


void resume_vm(struct arg_node * args) {

    struct arg_node * node = get_arg(args, 0);
    struct vm_identifier_table_row * vmid = new_vmid_search_result_by_name(node->arg.str);

    if ((!vmid) || (!(vmid->path))) {
        xcpmd_log(LOG_WARNING, "Failed to resume vm %s--couldn't get UUID\n", node->arg.str);
        free_vmid_search_result(vmid);
        return;
    }

    dbus_async_call("com.citrix.xenclient.xenmgr", vmid->path, "com.citrix.xenclient.xenmgr.vm", com_citrix_xenclient_xenmgr_vm_resume_async, NULL);
    free_vmid_search_result(vmid);
}


void pause_vm(struct arg_node * args) {

    struct arg_node * node = get_arg(args, 0);
    struct vm_identifier_table_row * vmid = new_vmid_search_result_by_name(node->arg.str);

    if ((!vmid) || (!(vmid->path))) {
        xcpmd_log(LOG_WARNING, "Failed to pause vm %s--couldn't get UUID\n", node->arg.str);
        free_vmid_search_result(vmid);
        return;
    }

    dbus_async_call("com.citrix.xenclient.xenmgr", vmid->path, "com.citrix.xenclient.xenmgr.vm", com_citrix_xenclient_xenmgr_vm_pause_async, NULL);
    free_vmid_search_result(vmid);
}


void unpause_vm(struct arg_node * args) {

    struct arg_node * node = get_arg(args, 0);
    struct vm_identifier_table_row * vmid = new_vmid_search_result_by_name(node->arg.str);

    if ((!vmid) || (!(vmid->path))) {
        xcpmd_log(LOG_WARNING, "Failed to unpause vm %s--couldn't get UUID\n", node->arg.str);
        free_vmid_search_result(vmid);
        return;
    }

    dbus_async_call("com.citrix.xenclient.xenmgr", vmid->path, "com.citrix.xenclient.xenmgr.vm", com_citrix_xenclient_xenmgr_vm_unpause_async, NULL);
    free_vmid_search_result(vmid);
}


void reboot_vm(struct arg_node * args) {

    struct arg_node * node = get_arg(args, 0);
    struct vm_identifier_table_row * vmid = new_vmid_search_result_by_name(node->arg.str);

    if ((!vmid) || (!(vmid->path))) {
        xcpmd_log(LOG_WARNING, "Failed to reboot vm %s--couldn't get UUID\n", node->arg.str);
        free_vmid_search_result(vmid);
        return;
    }

    dbus_async_call("com.citrix.xenclient.xenmgr", vmid->path, "com.citrix.xenclient.xenmgr.vm", com_citrix_xenclient_xenmgr_vm_reboot_async, NULL);
    free_vmid_search_result(vmid);
}


void shutdown_vm(struct arg_node * args) {

    struct arg_node * node = get_arg(args, 0);
    struct vm_identifier_table_row * vmid = new_vmid_search_result_by_name(node->arg.str);

    if ((!vmid) || (!(vmid->path))) {
        xcpmd_log(LOG_WARNING, "Failed to shutdown vm %s--couldn't get UUID\n", node->arg.str);
        free_vmid_search_result(vmid);
        return;
    }

    dbus_async_call("com.citrix.xenclient.xenmgr", vmid->path, "com.citrix.xenclient.xenmgr.vm", com_citrix_xenclient_xenmgr_vm_shutdown_async, NULL);
    free_vmid_search_result(vmid);
}


void start_vm(struct arg_node * args) {

    struct arg_node * node = get_arg(args, 0);
    struct vm_identifier_table_row * vmid = new_vmid_search_result_by_name(node->arg.str);

    if ((!vmid) || (!(vmid->path))) {
        xcpmd_log(LOG_WARNING, "Failed to start vm %s--couldn't get UUID\n", node->arg.str);
        free_vmid_search_result(vmid);
        return;
    }

    dbus_async_call("com.citrix.xenclient.xenmgr", vmid->path, "com.citrix.xenclient.xenmgr.vm", com_citrix_xenclient_xenmgr_vm_start_async, NULL);
    free_vmid_search_result(vmid);
}


void suspend_vm_to_file(struct arg_node * args) {

    struct arg_node * node = get_arg(args, 0);
    struct vm_identifier_table_row * vmid = new_vmid_search_result_by_name(node->arg.str);

    if ((!vmid) || (!(vmid->path))) {
        xcpmd_log(LOG_WARNING, "Failed to suspend vm %s to file--couldn't get UUID\n", node->arg.str);
        free_vmid_search_result(vmid);
        return;
    }

    node = get_arg(args, 1);
    char * filename = node->arg.str;

    dbus_async_call_with_arg("com.citrix.xenclient.xenmgr", vmid->path, "com.citrix.xenclient.xenmgr.vm", com_citrix_xenclient_xenmgr_vm_suspend_to_file_async, NULL, filename);
    free_vmid_search_result(vmid);
}


void resume_vm_from_file(struct arg_node * args) {

    struct arg_node * node = get_arg(args, 0);
    struct vm_identifier_table_row * vmid = new_vmid_search_result_by_name(node->arg.str);

    if ((!vmid) || (!(vmid->path))) {
        xcpmd_log(LOG_WARNING, "Failed to resume vm %s from file--couldn't get UUID\n", node->arg.str);
        free_vmid_search_result(vmid);
        return;
    }

    node = get_arg(args, 1);
    char * filename = node->arg.str;

    dbus_async_call_with_arg("com.citrix.xenclient.xenmgr", vmid->path, "com.citrix.xenclient.xenmgr.vm", com_citrix_xenclient_xenmgr_vm_resume_from_file_async, NULL, filename);
    free_vmid_search_result(vmid);
}


void sleep_vm_by_uuid (struct arg_node * args) {

    struct arg_node * node = get_arg(args, 0);
    struct vm_identifier_table_row * vmid = new_vmid_search_result_by_uuid(node->arg.str);

    if ((!vmid) || (!(vmid->path))) {
        xcpmd_log(LOG_WARNING, "Failed to sleep vm--couldn't get xenstore path\n");
        free_vmid_search_result(vmid);
    return;
    }

    dbus_async_call("com.citrix.xenclient.xenmgr", vmid->path, "com.citrix.xenclient.xenmgr.vm", com_citrix_xenclient_xenmgr_vm_sleep_async, NULL);
    free_vmid_search_result(vmid);
}


void resume_vm_by_uuid (struct arg_node * args) {

    struct arg_node * node = get_arg(args, 0);
    struct vm_identifier_table_row * vmid = new_vmid_search_result_by_uuid(node->arg.str);

    if ((!vmid) || (!(vmid->path))) {
        xcpmd_log(LOG_WARNING, "Failed to resume vm--couldn't get xenstore path\n");
        free_vmid_search_result(vmid);
    return;
    }

    dbus_async_call("com.citrix.xenclient.xenmgr", vmid->path, "com.citrix.xenclient.xenmgr.vm", com_citrix_xenclient_xenmgr_vm_resume_async, NULL);
    free_vmid_search_result(vmid);
}


void pause_vm_by_uuid (struct arg_node * args) {

    struct arg_node * node = get_arg(args, 0);
    struct vm_identifier_table_row * vmid = new_vmid_search_result_by_uuid(node->arg.str);

    if ((!vmid) || (!(vmid->path))) {
        xcpmd_log(LOG_WARNING, "Failed to pause vm--couldn't get xenstore path\n");
        free_vmid_search_result(vmid);
    return;
    }

    dbus_async_call("com.citrix.xenclient.xenmgr", vmid->path, "com.citrix.xenclient.xenmgr.vm", com_citrix_xenclient_xenmgr_vm_pause_async, NULL);
    free_vmid_search_result(vmid);
}


void unpause_vm_by_uuid (struct arg_node * args) {

    struct arg_node * node = get_arg(args, 0);
    struct vm_identifier_table_row * vmid = new_vmid_search_result_by_uuid(node->arg.str);

    if ((!vmid) || (!(vmid->path))) {
        xcpmd_log(LOG_WARNING, "Failed to unpause vm--couldn't get xenstore path\n");
        free_vmid_search_result(vmid);
    return;
    }

    dbus_async_call("com.citrix.xenclient.xenmgr", vmid->path, "com.citrix.xenclient.xenmgr.vm", com_citrix_xenclient_xenmgr_vm_unpause_async, NULL);
    free_vmid_search_result(vmid);
}


void reboot_vm_by_uuid (struct arg_node * args) {

    struct arg_node * node = get_arg(args, 0);
    struct vm_identifier_table_row * vmid = new_vmid_search_result_by_uuid(node->arg.str);

    if ((!vmid) || (!(vmid->path))) {
        xcpmd_log(LOG_WARNING, "Failed to reboot vm--couldn't get xenstore path\n");
        free_vmid_search_result(vmid);
    return;
    }

    dbus_async_call("com.citrix.xenclient.xenmgr", vmid->path, "com.citrix.xenclient.xenmgr.vm", com_citrix_xenclient_xenmgr_vm_reboot_async, NULL);
    free_vmid_search_result(vmid);
}


void shutdown_vm_by_uuid (struct arg_node * args) {

    struct arg_node * node = get_arg(args, 0);
    struct vm_identifier_table_row * vmid = new_vmid_search_result_by_uuid(node->arg.str);

    if ((!vmid) || (!(vmid->path))) {
        xcpmd_log(LOG_WARNING, "Failed to shutdown vm--couldn't get xenstore path\n");

        if (vmid != NULL)
            free_vmid_search_result(vmid);
    return;
    }

    dbus_async_call("com.citrix.xenclient.xenmgr", vmid->path, "com.citrix.xenclient.xenmgr.vm", com_citrix_xenclient_xenmgr_vm_shutdown_async, NULL);
    free_vmid_search_result(vmid);
}


void start_vm_by_uuid (struct arg_node * args) {

    struct arg_node * node = get_arg(args, 0);
    struct vm_identifier_table_row * vmid = new_vmid_search_result_by_uuid(node->arg.str);

    if ((!vmid) || (!(vmid->path))) {
        xcpmd_log(LOG_WARNING, "Failed to start vm--couldn't get xenstore path\n");

        if (vmid != NULL)
            free_vmid_search_result(vmid);
    return;
    }

    dbus_async_call("com.citrix.xenclient.xenmgr", vmid->path, "com.citrix.xenclient.xenmgr.vm", com_citrix_xenclient_xenmgr_vm_start_async, NULL);
    free_vmid_search_result(vmid);
}


void suspend_vm_by_uuid_to_file (struct arg_node * args) {

    struct arg_node * node = get_arg(args, 0);
    struct vm_identifier_table_row * vmid = new_vmid_search_result_by_uuid(node->arg.str);

    if ((!vmid) || (!(vmid->path))) {
        xcpmd_log(LOG_WARNING, "Failed to shutdown vm--couldn't get xenstore path\n");

        if (vmid != NULL)
            free_vmid_search_result(vmid);
    return;
    }

    node = get_arg(args, 1);
    char * filename = node->arg.str;

    dbus_async_call_with_arg("com.citrix.xenclient.xenmgr", vmid->path, "com.citrix.xenclient.xenmgr.vm", com_citrix_xenclient_xenmgr_vm_suspend_to_file_async, NULL, filename);
    free_vmid_search_result(vmid);
}


void resume_vm_by_uuid_from_file (struct arg_node * args) {

    struct arg_node * node = get_arg(args, 0);
    struct vm_identifier_table_row * vmid = new_vmid_search_result_by_uuid(node->arg.str);

    if ((!vmid) || (!(vmid->path))) {
        xcpmd_log(LOG_WARNING, "Failed to shutdown vm--couldn't get xenstore path\n");
        free_vmid_search_result(vmid);
    return;
    }

    node = get_arg(args, 1);
    char * filename = node->arg.str;

    dbus_async_call_with_arg("com.citrix.xenclient.xenmgr", vmid->path, "com.citrix.xenclient.xenmgr.vm", com_citrix_xenclient_xenmgr_vm_resume_from_file_async, NULL, filename);
    free_vmid_search_result(vmid);
}


//Helper function for shutdown_dependencies_of_vm().
//Allocs every entry in flat_list and clones a vm_path string for each.
static void gather_dependencies(char * vm_to_check, struct vm_deps * master_deps, struct vm_list * flat_list) {

    //find vm in master list
    //add its deps to flat list
    //call again on each dependency

    struct vm_deps * vm_with_deps;
    struct vm_list * flat_list_entry;
    unsigned int i;
    bool entry_in_list;
    static unsigned int depth = 0;

    //Look up dependencies and state of vm_to_check.
    list_for_each_entry(vm_with_deps, &master_deps->list, list) {

        if (!strcmp(vm_with_deps->vm_path, vm_to_check)) {

            //Skip this vm if it's stopped or stopping, unless it's the root vm.
            if (depth > 0 && (!strcmp(vm_with_deps->vm_state, "stopping") || !strcmp(vm_with_deps->vm_state, "stopped"))) {
                //xcpmd_log(LOG_DEBUG, "Omitting %s from jeopardy list, since it's %s.", vm_with_deps->vm_path, vm_with_deps->vm_state);
                return;
            }

            for (i=0; i < vm_with_deps->deps->num_entries; ++i) {
                //if this dep is already in list, pass
                entry_in_list = false;
                list_for_each_entry(flat_list_entry, &flat_list->list, list) {
                    if (!strcmp(flat_list_entry->vm_path, vm_with_deps->deps->entries[i].path)) {
                        entry_in_list = true;
                        break;
                    }
                }

                if (!entry_in_list) {
                    flat_list_entry = (struct vm_list *)malloc(sizeof(struct vm_list));
                    flat_list_entry->vm_path = clone_string(vm_with_deps->deps->entries[i].path);
                    list_add_tail(&flat_list_entry->list, &flat_list->list);
                    //xcpmd_log(LOG_DEBUG, "Adding %s to jeopardy list.", flat_list_entry->vm_path);
                    ++depth;
                    gather_dependencies(flat_list_entry->vm_path, master_deps, flat_list);
                    --depth;
                }
            }
        }
    }
}


//Shuts down all dependencies of the VM at the xenstore path specified.
//Optionally, provide a type to shut down only dependencies of that type.
//Does not attempt to shut down VMs that are already stopping/stopped, or
//any VMs that are still depended on by other VMs.
void shutdown_dependencies_of_vm(char * vm_path, char * type) {

    GPtrArray * tmp;
    char * state;
    struct vm_identifier_table * safe_entry_deps = NULL;
    unsigned int i;

    if (!vm_path) {
        return;
    }

    //work with fresh data
    populate_vm_identifier_table();

    //gather all vm dependency data
    struct vm_deps vm_deps_list, *deps_list_entry, *deps_list_ptr;
    struct vm_list safe, jeopardy, *jeopardy_list_entry, *safe_list_entry, *vm_list_ptr;

    INIT_LIST_HEAD(&vm_deps_list.list);
    INIT_LIST_HEAD(&safe.list);
    INIT_LIST_HEAD(&jeopardy.list);

    //Cache master list of vms and their state and dependencies.
    for (i=0; i < vm_identifier_table->num_entries; ++i) {

        deps_list_entry = (struct vm_deps *)malloc(sizeof(struct vm_deps));
        list_add_tail(&deps_list_entry->list, &vm_deps_list.list);

        deps_list_entry->vm_path = clone_string(vm_identifier_table->entries[i].path);

        property_get_com_citrix_xenclient_xenmgr_vm_state_(xcdbus_conn, XENMGR_SERVICE, vm_identifier_table->entries[i].path, &state);
        deps_list_entry->vm_state = clone_string(state);

        get_vm_dependencies(deps_list_entry->vm_path, &tmp);
        deps_list_entry->deps = new_vm_identifier_table(tmp);
    }

    //Add all dependencies of the vm to a jeopardy list.
    gather_dependencies(vm_path, &vm_deps_list, &jeopardy);

    //Generate the complement of the jeopardy list, the safe list.
    list_for_each_entry(deps_list_entry, &vm_deps_list.list, list) {

        //But omit the VM who this function was called on.
        if (!strcmp(vm_path, deps_list_entry->vm_path)) {
            continue;
        }

        //Also omit VMs who are stopping/stopped.
        if (!strcmp(deps_list_entry->vm_state, "stopping") || !strcmp(deps_list_entry->vm_state, "stopped")) {
            //xcpmd_log(LOG_DEBUG, "Omitting %s from safe list, since it's %s.", deps_list_entry->vm_path, deps_list_entry->vm_state);
            continue;
        }

        bool in_jeopardy = false;
        list_for_each_entry(jeopardy_list_entry, &jeopardy.list, list) {
            if (!strcmp(jeopardy_list_entry->vm_path, deps_list_entry->vm_path)) {
                in_jeopardy = true;
                break;
            }
        }

        if (!in_jeopardy) {
            safe_list_entry = (struct vm_list *)malloc(sizeof(struct vm_list));
            safe_list_entry->vm_path = clone_string(deps_list_entry->vm_path);
            list_add_tail(&safe_list_entry->list, &safe.list);
            //xcpmd_log(LOG_DEBUG, "Adding %s to safe list.", safe_list_entry->vm_path);
        }
    }

    //Does any VM in the safe list depend on any VM in the jeopardy list?
    list_for_each_entry(safe_list_entry, &safe.list, list) {

        //Find the safe_list_entry's dependencies
        list_for_each_entry(deps_list_entry, &vm_deps_list.list, list) {
            if (!strcmp(deps_list_entry->vm_path, safe_list_entry->vm_path)) {
                safe_entry_deps = deps_list_entry->deps;
                break;
            }
        }

        if (safe_entry_deps == NULL) {
            xcpmd_log(LOG_DEBUG, "Couldn't get deps of %s midfunction--did something free the dependency cache?", deps_list_entry->vm_path);
            return;
        }

        //Check if any of those dependencies are in jeopardy
        for (i=0; i < safe_entry_deps->num_entries; ++i) {
            list_for_each_entry_safe(jeopardy_list_entry, vm_list_ptr, &jeopardy.list, list) {
                //If so, move them to the safe list.
                if (!strcmp(safe_entry_deps->entries[i].path, jeopardy_list_entry->vm_path)) {
                    list_del(&jeopardy_list_entry->list);
                    list_add_tail(&jeopardy_list_entry->list, &safe.list);
                    //xcpmd_log(LOG_DEBUG, "Moving %s from jeopardy list to safe list, since %s depends on it", jeopardy_list_entry->vm_path, safe_list_entry->vm_path);
                    break;
                }
            }
        }
    }

    //Anything remaining in the jeopardy list at this point is ready to be shut down.
    list_for_each_entry_safe(jeopardy_list_entry, vm_list_ptr, &jeopardy.list, list) {

        //xcpmd_log(LOG_DEBUG, "Shutting down %s.", jeopardy_list_entry->vm_path);
        shutdown_vm_async(jeopardy_list_entry->vm_path);
        list_del(&jeopardy_list_entry->list);
        free(jeopardy_list_entry->vm_path);
        free(jeopardy_list_entry);
    }

    //Free the safe list.
    list_for_each_entry_safe(safe_list_entry, vm_list_ptr, &safe.list, list) {
        list_del(&safe_list_entry->list);
        free(safe_list_entry->vm_path);
        free(safe_list_entry);
    }

    //Free the VM dependencies list.
    list_for_each_entry_safe(deps_list_entry, deps_list_ptr, &vm_deps_list.list, list) {
        list_del(&deps_list_entry->list);
        free_vm_identifier_table(deps_list_entry->deps);
        free(deps_list_entry->vm_path);
        free(deps_list_entry->vm_state);
        free(deps_list_entry);
    }
}


void shutdown_dependencies_of_vm_by_name (struct arg_node * args) {

    struct arg_node * node = get_arg(args, 0);
    struct vm_identifier_table_row * vmid = new_vmid_search_result_by_name(node->arg.str);

    vmid = new_vmid_search_result_by_name(node->arg.str);
    if (vmid && vmid->path) {
        shutdown_dependencies_of_vm(vmid->path, NULL);
    }
    free_vmid_search_result(vmid);
}


void shutdown_dependencies_of_vm_by_uuid (struct arg_node * args) {

    struct arg_node * node = get_arg(args, 0);
    struct vm_identifier_table_row * vmid = new_vmid_search_result_by_name(node->arg.str);

    vmid = new_vmid_search_result_by_uuid(node->arg.str);
    if (vmid && vmid->path) {
        shutdown_dependencies_of_vm(vmid->path, NULL);
    }
    free_vmid_search_result(vmid);
}


void shutdown_vpnvm_dependencies_of_vm_by_name (struct arg_node * args) {

    struct arg_node * node = get_arg(args, 0);
    struct vm_identifier_table_row * vmid = new_vmid_search_result_by_name(node->arg.str);

    vmid = new_vmid_search_result_by_name(node->arg.str);
    if (vmid && vmid->path) {
        shutdown_dependencies_of_vm(vmid->path, "vpnvm");
    }
    free_vmid_search_result(vmid);
}


void shutdown_vpnvm_dependencies_of_vm_by_uuid (struct arg_node * args) {

    struct arg_node * node = get_arg(args, 0);
    struct vm_identifier_table_row * vmid = new_vmid_search_result_by_name(node->arg.str);

    vmid = new_vmid_search_result_by_uuid(node->arg.str);
    if (vmid && vmid->path) {
        shutdown_dependencies_of_vm(vmid->path, "vpnvm");
    }
    free_vmid_search_result(vmid);
}
