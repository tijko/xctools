/*
 * Copyright (c) 2014 Citrix Systems, Inc.
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

#include <sys/types.h>
#include <sys/socket.h>
#include <linux/types.h>
#include <linux/netlink.h>
#include <linux/if.h>
#include <netlink/object-api.h>
#include <netlink/route/addr.h>
#include <netlink/route/rtnl.h>
#include <netlink/route/link.h>
#include <stdio.h>

#define MAX_IF 4096

static uint8_t carrier[MAX_IF];

static void
print(char *name, int c)
{
        printf("interface %s carrier %d\n", name, c);
        fflush(stdout);
}

static void
link_msg_handler (struct nl_object *obj, void *arg)
{
        struct rtnl_link *filter;
        struct rtnl_link *link_obj;
        unsigned int flags, ifidx;
        uint8_t c;
        char *name;

        (void)arg;

        filter = rtnl_link_alloc();
        if (!filter) {
                return;
        }

        /* Ensure it's a link object */
        if (nl_object_match_filter(obj, OBJ_CAST (filter)) == 0) {
                rtnl_link_put (filter);
                return;
        }

        link_obj = (struct rtnl_link *) obj;
        flags = rtnl_link_get_flags(link_obj);
        name = rtnl_link_get_name(link_obj);
        ifidx = rtnl_link_get_ifindex(link_obj);
        c = (flags & IFF_LOWER_UP) ? 1:0;

        if (ifidx >= MAX_IF)
                print(name, c);
        else
                if (carrier[ifidx] != c) {
                        carrier[ifidx] = c;
                        print(name, c);
                }

        rtnl_link_put(filter);
}

static int
event_msg_valid (struct nl_msg *msg, void *arg)
{
        (void)msg;
        (void)arg;

        /* Parse carrier messages */
        nl_msg_parse(msg, &link_msg_handler, NULL);
        return NL_OK;
}

static int noop_seq_check(struct nl_msg *msg, void *arg)
{
        (void)msg;
        (void)arg;
        return NL_OK;
}

int main(void)
{
        struct nl_sock *sock;
        int err;

        memset(carrier, 0xFF, sizeof(carrier));

        sock = nl_socket_alloc();
        if (!sock) {
                fprintf(stderr, "[Err] Unable to allocate netlink socket.\n");
                return 1;
        }

        err = nl_connect(sock, NETLINK_ROUTE);
        if (err < 0) {
                fprintf(stderr, "Unable to connect socket");
                return 1;
        }

        nl_socket_modify_cb(sock, NL_CB_SEQ_CHECK, NL_CB_CUSTOM, noop_seq_check, NULL);
        nl_socket_modify_cb(sock, NL_CB_VALID, NL_CB_CUSTOM, event_msg_valid, NULL);
        nl_socket_add_membership(sock, RTNLGRP_LINK);

        for (;;) {
                nl_recvmsgs_default(sock);
        }
}
