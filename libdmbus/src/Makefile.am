#
#
# Makefile.am:
#
# Copyright (c) 2010 Julian Pidancet <julian.pidancet@citrix.com>,
# All rights reserved.
#
# $Id:$
#
# $Log:$
#
#
#

#
# Copyright (c) 2012 Citrix Systems, Inc.
# 
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.
# 
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
# 
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
#


INCLUDES = 

SRCS = dmbus.c

DMBUSSRCS=${SRCS}

noinst_HEADERS = project.h

libdmbus_la_SOURCES = ${DMBUSSRCS}
libdmbus_la_LDFLAGS = \
	-version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE) \
	-release $(LT_RELEASE) \
	-export-dynamic

BUILT_SOURCES = libdmbus.h dmbus.c dmbus.sha1

lib_LTLIBRARIES = libdmbus.la

include_HEADERS = libdmbus.h
AM_CFLAGS = -g

VFD = ${srcdir}/..
VF = ${shell cat ${VFD}/version-files}
VFS = ${VF:%=${VFD}/%}
VCHK = ${shell cat ${VFS} | @MD5SUM@ | @AWK@ '{ print $$1 }' }
VNUM = ${shell @GREP@ ${VCHK} ${VFD}/version-md5sums | @AWK@ '{ print $$2 }'  }
VDEF = ${shell echo `cat ${VFD}/version-major`.`cat ${VFD}/version-minor`.`cat ${VFD}/version-micro` }

dmbus.c: dmbus.c.in rpc_definitions.m4 rpcgen.m4 dmbus.sha1
	cat rpc_definitions.m4 dmbus.c.in | m4 > dmbus.c
libdmbus.h: libdmbus.h.in rpc_definitions.m4 rpcgen.m4 dmbus.sha1
	cat rpc_definitions.m4 libdmbus.h.in | m4 > libdmbus.h
dmbus.sha1: rpc_definitions.m4 rpcgen.m4 libdmbus.h.in dmbus.c.in
	cat $^ | ${SHA1SUM} - | cut -d ' ' -f 1 > dmbus.sha1

version.h: $(VFD)/version-files $(VFD)/version-major \
	$(VFD)/version-minor $(VFD)/version-micro \
	$(VFD)/version-md5sums ${VFS} Makefile
	if [ .${VNUM} = . ]; then \
		echo "#define FULL_VERSION \"libdmbus Version ${VDEF} + Edits\"" > version.h; \
		echo ${VDEF}-E > version-num; \
	else \
		echo "#define FULL_VERSION \"libdmbus Version ${VNUM}\"" > version.h; \
		echo ${VNUM} > version-num; \
	fi
