#
#
# Makefile.am:
#
# Copyright (c) 2011 James McKenzie <20@madingley.org>,
# All rights reserved.
#
# $Id:$
#
# $Log:$
#
#
#

#
# Copyright (c) 2011 Citrix Systems, Inc.
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


CPROTO=cproto
INCLUDES = ${LIBPCIACCESS_INC}

noinst_HEADERS=project.h prototypes.h

bin_PROGRAMS = inquart

SRCS=inquart.c version.c
inquart_SOURCES = ${SRCS}
inquart_LDADD = ${LIBPCIACCESS_LIB}

AM_CFLAGS=-g

inquart_LDFLAGS = 

BUILT_SOURCES = version.h


VFD=${srcdir}/..
VF=${shell cat ${VFD}/version-files}
VFS=${VF:%=${VFD}/%}
VCHK=${shell cat ${VFS} | @MD5SUM@ | @AWK@ '{print $$1 }' }
VNUM=${shell @GREP@ ${VCHK} ${VFD}/version-md5sums | @AWK@ '{ print $$2 }'  }
VDEF=${shell echo `cat ${VFD}/version-major`.`cat ${VFD}/version-minor`.`cat ${VFD}/version-micro` }

protos:
	echo > prototypes.h
	${CPROTO} -v ${INCLUDES} ${SRCS} > prototypes.tmp
	mv -f prototypes.tmp prototypes.h



version.h: $(VFD)/version-files $(VFD)/version-major \
	$(VFD)/version-minor $(VFD)/version-micro \
	$(VFD)/version-md5sums ${VFS} Makefile
	if [ .${VNUM} = . ]; then \
		echo "#define VERSION \"inquart Version ${VDEF} + Edits\"" > version.h; \
		echo ${VDEF}-E > version-num; \
	else \
		echo "#define VERSION \"inquart Version ${VNUM}\"" > version.h; \
		echo ${VNUM} > version-num; \
	fi

