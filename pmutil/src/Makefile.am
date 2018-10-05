#
# Copyright (c) 2012 Citrix Systems, Inc.
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

#
#
# Makefile.am:
#
#

CPROTO=cproto
INCLUDES =  ${LIBXENSTORE_INC} ${LIBXC_INC} ${LIBXCDBUS_INC} ${LIBXENACPI_INC} ${DBUS_GLIB_1_INC} ${GLIB_20_INC}

DBUS_CLIENT_IDLS=surfman 

noinst_HEADERS=project.h prototypes.h pmutil.h

bin_PROGRAMS = pmutil

BUILT_SOURCES = ${DBUS_CLIENT_IDLS:%=rpcgen/%_client.h} \
	version.h 

SRCS=pmutil.c acpi-decode.c dmar-trace.c bcl.c wmi.c version.c utils.c
pmutil_SOURCES = ${SRCS}
pmutil_LDADD = -lpci ${LIBXENSTORE_LIB} ${LIBXC_LIB} ${LIBXCDBUS_LIB} ${LIBXENACPI_LIB} ${DBUS_GLIB_1_LIB} ${GLIB_20_LIB}

AM_CFLAGS=-g

pmutil_LDFLAGS = 

VFD=${srcdir}/..
VF=${shell cat ${VFD}/version-files}
VFS=${VF:%=${VFD}/%}
VCHK=${shell cat ${VFS} | @MD5SUM@ | @AWK@ '{print $$1 }' }
VNUM=${shell @GREP@ ${VCHK} ${VFD}/version-md5sums | @AWK@ '{ print $$2 }'  }
VDEF=${shell echo `cat ${VFD}/version-major`.`cat ${VFD}/version-minor`.`cat ${VFD}/version-micro` }

rpcgen/%_client.h: ${IDLDIR}/%.xml
	mkdir rpcgen || true
	${XC_RPCGEN} -c -o rpcgen/ $<


protos:
	echo > prototypes.h
	${CPROTO} -v ${INCLUDES} ${SRCS} > prototypes.tmp
	mv -f prototypes.tmp prototypes.h


version.h: $(VFD)/version-files $(VFD)/version-major \
	$(VFD)/version-minor $(VFD)/version-micro \
	$(VFD)/version-md5sums ${VFS} Makefile
	if [ .${VNUM} = . ]; then \
		echo "#define VERSION \"pmutil Version ${VDEF} + Edits\"" > version.h; \
		echo ${VDEF}-E > version-num; \
	else \
		echo "#define VERSION \"pmutil Version ${VNUM}\"" > version.h; \
		echo ${VNUM} > version-num; \
	fi

