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

#include "parser.h"
#include "rules.h"


//Used by the parser:
bool parse_db_vars(struct parse_data * data);
bool parse_db_rules(struct parse_data * data);

//General use:
void write_db_rule(struct rule * rule);
void write_db_rules();
void delete_db_rule(char * rule_name);
void delete_db_rules();

//Access variables through a write-through cache:
struct db_var * lookup_var(char * name);
struct arg_node * resolve_var(char * name);
struct db_var * add_var(char * name, enum arg_type type, union arg_u value, char ** parse_error);
int delete_var(char * name);
void delete_vars();

//Tear down the cache:
void delete_cached_vars();
