/*
 * utils.c
 *
 * PM utility common utilities
 *
 * Copyright (c) 2011 Ross Philipson <ross.philipson@citrix.com>
 * Copyright (c) 2011  Citrix Systems, Inc.
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

#include "project.h"
#include <sys/mman.h>
#include <ctype.h>
#include <fcntl.h>
#include "pmutil.h"

int strnicmp(const char *s1, const char *s2, size_t len)
{
    unsigned char c1, c2;

    c1 = c2 = 0;
    if ( len == 0 )
        return 0;

    do
    {
        c1 = *s1;
        c2 = *s2;
        s1++;
        s2++;

        if ( (c1 == 0) || (c2 == 0) )
            break;        
        if ( c1 == c2 )
            continue;

        c1 = tolower(c1);
        c2 = tolower(c2);
        if ( c1 != c2 )
            break;
    } while ( --len );

    return (int)c1 - (int)c2;
}

int file_set_blocking(int fd)
{
    long arg;

    arg = (long)fcntl(fd, F_GETFL, arg);
    arg &= ~O_NONBLOCK;
    return fcntl(fd, F_SETFL, arg);
}

int file_set_nonblocking(int fd)
{
    long arg;

    arg = (long)fcntl(fd, F_GETFL, arg);
    arg |= O_NONBLOCK;
    return fcntl(fd, F_SETFL, arg);
}

uint8_t *map_phys_mem(size_t phys_addr, size_t length)
{
    uint32_t page_offset = phys_addr % sysconf(_SC_PAGESIZE);
    uint8_t *addr;
    int fd;

    fd = open("/dev/mem", O_RDONLY);
    if ( fd == -1 )
        return NULL;

    addr = (uint8_t*)mmap(0, page_offset + length, PROT_READ, MAP_PRIVATE, fd, phys_addr - page_offset);
    close(fd);

    if ( addr == MAP_FAILED )
        return NULL;      

    return addr + page_offset;
}

void unmap_phys_mem(uint8_t *addr, size_t length)
{
    uint32_t page_offset = (size_t)addr % sysconf(_SC_PAGESIZE);

    munmap(addr - page_offset, length + page_offset);
}

int efi_locate_entry(const char *efi_entry, uint32_t length, size_t *location)
{
#define EFI_LINE_SIZE 64
    FILE *systab = NULL;
    char efiline[EFI_LINE_SIZE];
    char *val;
    off_t loc = 0;

    *location = 0;

    /* use EFI tables if present */
    systab = fopen("/sys/firmware/efi/systab", "r");
    if ( systab != NULL )
    {
        while( (fgets(efiline, EFI_LINE_SIZE - 1, systab)) != NULL )
        {
            if ( strncmp(efiline, efi_entry, 6) == 0 ) 
            {
                /* found EFI entry, get the associated value */
                val = memchr(efiline, '=', strlen(efiline)) + 1;
                loc = strtol(val, NULL, 0);
                break;
            }
        }
        fclose(systab);

        if ( loc != 0 )
	{
            *location = loc;
            return 0;
        }
    }

    return -1;
}

char* xenstore_read_str(struct xs_handle *xs, char *path)
{
    char *buf;
    unsigned int len;

    buf = xs_read(xs, XBT_NULL, path, &len);
    return buf;
}

unsigned int xenstore_read_uint(struct xs_handle *xs, char *path)
{
    char *buf;
    int ret;

    buf = xenstore_read_str(xs, path);
    if ( buf == NULL)
        return 0;

    ret  = atoi(buf);
    free(buf);
    return ret;
}

