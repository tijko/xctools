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

#ifndef __PARSER_H__
#define __PARSER_H__

#include "rules.h"

//Parse errors
#define NO_PARSE_ERROR      0x000
#define RULE_INVALID        0x100
#define FSM_ERROR           0x200
#define BOOL_ERROR          0x300
#define VAR_MISSING         0x400
#define INVALID_VAR_TYPE    0x500
#define OVERLAPPING_VARS    0x600
#define RECOVERABLE_ERROR   0x700
#define DB_ERROR            0x800
#define MEMORY_ERROR        0x900

struct parse_data;

int parse_config_from_db();
int parse_config_from_file(char * filename);

bool parse_rule_persistent(struct parse_data * data, char * name, char * conditions, char * actions, char * undos);
bool parse_var_persistent(struct parse_data * data, char * var_string);

bool parse_rule(char * name, char * conditions, char * actions, char * undos, char ** error);
bool parse_var(char * var_string, char ** error);
bool parse_arg(char * arg_string, struct arg_node * arg_out, char ** error);

char * extract_parse_error(struct parse_data * data);

void external_error(struct parse_data * data, int error_code, char * error_string);

#endif
