#
#
# Makefile.am:
#
# $Id:$
#
# $Log:$
#
#
#

INCLUDES =

SRCS= libxenacpi.c version.c wmi.c vid.c amlcore.c amlgen.c amlres.c
CPROTO=cproto

XENACPISRCS=${SRCS}

noinst_HEADERS= project.h prototypes.h xenacpi-tail.h ext_prototypes.h

libxenacpi_la_SOURCES = ${XENACPISRCS}

BUILT_SOURCES = version.h xenacpi.h

lib_LTLIBRARIES=libxenacpi.la

include_HEADERS=xenacpi.h

AM_CFLAGS=-g

libxenacpi_la_LDFLAGS = \
        -version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE) \
        -release $(LT_RELEASE) \
        -export-dynamic

VFD=${srcdir}/..
VF=${shell cat ${VFD}/version-files}
VFS=${VF:%=${VFD}/%}
VCHK=${shell cat ${VFS} | @MD5SUM@ | @AWK@ '{print $$1 }' }
VNUM=${shell @GREP@ ${VCHK} ${VFD}/version-md5sums | @AWK@ '{ print $$2 }'  }
VDEF=${shell echo `cat ${VFD}/version-major`.`cat ${VFD}/version-minor`.`cat ${VFD}/version-micro` }


xenacpi.h:xenacpi-head.h xenacpi-defs.h ext_prototypes.h xenacpi-tail.h
	cat xenacpi-head.h xenacpi-defs.h ext_prototypes.h xenacpi-tail.h > xenacpi.h

protos:
	echo > prototypes.h
	echo > ext_prototypes.h
	cat xenacpi-head.h xenacpi-defs.h ext_prototypes.h xenacpi-tail.h > xenacpi.h
	${CPROTO} -v -DINT_PROTOS ${INCLUDES} ${SRCS} > prototypes.tmp
	mv -f prototypes.tmp prototypes.h
	${CPROTO} -v -DEXT_PROTOS ${INCLUDES} ${SRCS} > ext_prototypes.h
	cat xenacpi-head.h xenacpi-defs.h ext_prototypes.h xenacpi-tail.h > xenacpi.h

version.h: $(VFD)/version-files $(VFD)/version-major \
	$(VFD)/version-minor $(VFD)/version-micro \
	$(VFD)/version-md5sums ${VFS} Makefile
	if [ .${VNUM} = . ]; then \
		echo "#define FULL_VERSION \"libxenacpi Version ${VDEF} + Edits\"" > version.h; \
		echo ${VDEF}-E > version-num; \
	else \
		echo "#define FULL_VERSION \"libxenacpi Version ${VNUM}\"" > version.h; \
		echo ${VNUM} > version-num; \
	fi

