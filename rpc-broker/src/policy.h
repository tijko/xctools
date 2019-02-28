/*
 * Copyright (c) 2019 Assured Information Security, Inc.
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

#define RULES_FILENAME "/etc/rpc-broker.rules"
#define RULES_MAX_LENGTH 256
#define RULE_MAX_LENGTH  512

struct rule {
    uint8_t all:1;
    uint8_t out:1;
    uint8_t policy:1;
    uint8_t stubdom:1;
    uint8_t if_bool_flag:1;
    const char *destination;
    const char *path;
    const char *interface;
    const char *member;
    const char *if_bool;
    const char *domtype;
    const char *rule_string;
};

#define MAX_UUID       128
#define MAX_RULES      512
#define MAX_DOMAINS    128
#define MAX_SIGNALS     16
#define ETC_MAX_FILE 0xffff

#define TRANSFORM_UUID(uuid, uuid_buf)            \
 ({     size_t uuid_len = strlen(uuid);           \
        int i;                                    \
        for (i=0; i < uuid_len; i++) {            \
            if (((char *) uuid)[i] == '-')        \
                uuid_buf[i] = '_';                \
            else                                  \
                uuid_buf[i] = ((char *) uuid)[i]; \
        }                                         \
        uuid_buf[uuid_len] = '\0';                \
})                                                \

struct etc_policy {
    size_t count;
    struct rule rules[MAX_RULES];
};

struct domain_policy {
    uint16_t domid;
    size_t count;
    char uuid[MAX_UUID];
    char uuid_db_fmt[MAX_UUID];
    struct rule rules[MAX_RULES];
};

struct policy {
    size_t domain_count;
    time_t policy_load_time;
    size_t allowed_requests;
    size_t denied_requests;
    size_t total_requests;
    struct etc_policy etc;
    struct domain_policy domains[MAX_DOMAINS];
};


struct policy *dbus_broker_policy;

// src/policy.c
struct policy *build_policy(const char *rule_filepath);

void free_rule(struct rule r);

void free_policy(void);

