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

#ifndef __MODULES_H__
#define __MODULES_H__


struct ev_wrapper;


int init_modules();
void uninit_modules();
void * load_module(char * filename);
void unload_module(char * filename);
int load_modules(char ** module_list, unsigned int num_modules);
void unload_modules(char ** module_list, unsigned int num_modules);

struct ev_wrapper ** get_event_table(char * table_name, char * table_module);

void handle_events(struct ev_wrapper * event);

int load_policy_from_db();
int load_policy_from_file(char * filename);
void evaluate_policy();

bool policy_exists();

#endif
