/*
 * project.h
 *
 * XEN ACPI main project header.
 *
 * Copyright (c) 2012 Ross Philipson <ross.philipson@citrix.com>
 * Copyright (c) 2012 Citrix Systems, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

/*
 * $Id:$
 */

/*
 * $Log:$
 *
 */

#ifndef __PROJECT_H__
#define __PROJECT_H__

#include "config.h"

#ifdef TM_IN_SYS_TIME
#include <sys/time.h>
#ifdef TIME_WITH_SYS_TIME
#include <time.h>
#endif
#else
#ifdef TIME_WITH_SYS_TIME
#include <sys/time.h>
#endif
#include <time.h>
#endif

#include <stdio.h>
#include <stdlib.h>

#ifdef HAVE_MALLOC_H
#include <malloc.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#if defined(HAVE_STDINT_H)
#include <stdint.h>
#elif defined(HAVE_SYS_INT_TYPES_H)
#include <sys/int_types.h>
#endif

/* internal headers for talking to the Xen WMI helper driver */
#include <xen/public/acpi-wmi.h>

#ifdef INT_PROTOS
#define INTERNAL
#define EXTERNAL
#else
#ifdef EXT_PROTOS
#define INTERNAL static
#define EXTERNAL
#else
#define INTERNAL
#define EXTERNAL
#endif
#endif

#if !defined(__GNUC__)
#define INLINE __inline
#else
#define INLINE inline
#endif

#include <sys/types.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <errno.h>
#include <malloc.h>
#include <sys/stat.h>
#include <assert.h>

#include "xenacpi.h"

#include "prototypes.h"

static INLINE int xenacpi_error(int *e, int n)
{
    if (e != NULL)
    {
        if (n != 0)
            *e = n;
        else
            *e = -1;
    }
    return -1;
}

#endif /* __PROJECT_H__ */
