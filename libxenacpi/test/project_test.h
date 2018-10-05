/*
 * project_test.c
 *
 * XEN AML test header values
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

#ifndef __PROJECT_H__
#define __PROJECT_H__


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
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif

#include <windows.h>
#include "errno.h"

typedef ULONG64 uint64_t;
typedef ULONG32 uint32_t;
typedef USHORT uint16_t;
typedef UCHAR uint8_t;
typedef INT int32_t;
typedef CHAR int8_t;

#define _SC_PAGESIZE 1
static long sysconf(int v)
{
    v;
    return 4096;
}

void test_windows_wmi(void);

#define INLINE __inline

#pragma warning(disable: 4244)

#else

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <malloc.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <errno.h>
#include <assert.h>

#define INLINE inline

#endif

#include "xenacpi.h"

static int xenacpi_error(int *e, int n)
{
    if (e != NULL) {
        if (n != 0)
            *e = n;
        else
            *e = -1;
    }
    return -1;
}

#define LOG_ERR 1
#define LOG_INFO 2
static void xcpmd_log(int priority, const char *format, ...)
{
    va_list va;
    char buf[1024];

    va_start(va, format);
    vsprintf(buf, format, va);
    printf(buf);
    va_end(va);
}

void test_chain_peers(void **current, void **next);

int test_aml_gen(int argc, char* argv[]);
int test_aml_res(int argc, char* argv[]);

#endif /* __PROJECT_H__ */
