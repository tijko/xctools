#
# Copyright (c) 2014 Citrix Systems, Inc.
# 
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
#

BIN=xc-switcher

CFLAGS += -g -O2 -Wall -Wmissing-prototypes -D_GNU_SOURCE

# dbus
CFLAGS += $(shell pkg-config --cflags dbus-1)
LDFLAGS += $(shell pkg-config --libs dbus-1)
LDFLAGS += $(shell pkg-config --libs dbus-glib-1)

# gtk and gtk-x11
CFLAGS += $(shell pkg-config --cflags gtk+-2.0)
CFLAGS += $(shell pkg-config --cflags gtk+-x11-2.0)
#LDFLAGS += $(shell pkg-config --libs gtk+-2.0)
#LDFLAGS += $(shell pkg-config --libs gtk+-x11-2.0)

ifdef LIBAPPINDICATOR
# appindicator
CFLAGS += -DLIBAPPINDICATOR=1 $(shell pkg-config --cflags appindicator-0.1)
LDFLAGS += $(shell pkg-config --libs appindicator-0.1)
endif

# libnotify
CFLAGS += $(shell pkg-config --cflags libnotify)
CFLAGS += $(shell if pkg-config --atleast-version=0.7 libnotify; then echo "-DNEW_LIBNOTIFY"; fi)
LDFLAGS += $(shell pkg-config --libs libnotify)

SRCS = xc-switcher.c strbuf.c
OBJS = $(SRCS:.c=.o)

all: $(BIN)

$(BIN): $(OBJS)
	gcc -o $(BIN) $^ $(LDFLAGS)
clean:
	rm -f $(OBJS) $(BIN)
