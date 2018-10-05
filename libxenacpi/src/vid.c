/*
 * vid.c
 *
 * XEN ACPI Video access.
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

#include "project.h"
#include <sys/ioctl.h>

EXTERNAL int
xenacpi_vid_brightness_levels(struct xenacpi_vid_brightness_levels **levels_out,
                              int *error_out)
{
#define XENACPI_LEVELBUF_SIZE 512
    struct xenacpi_vid_brightness_levels *levels;
    char *list = NULL, *nptr, *eptr;
    FILE *fs = NULL;
    size_t length, i, total;
    int ret;

    if ( levels_out == NULL )
        return xenacpi_error(error_out, EINVAL);
    *levels_out = NULL;

    list = malloc(XENACPI_LEVELBUF_SIZE); /* will easily fit in this buffer */
    if ( list == NULL )
    {
        ret = xenacpi_error(error_out, ENOMEM);
        goto close_out;
    }
    memset(list, 0, XENACPI_LEVELBUF_SIZE);

    fs = fopen("/sys/module/video/parameters/brightness_levels", "r");
    if ( fs == NULL )
    {
        ret = xenacpi_error(error_out, errno);
        goto close_out;
    }

    length = fread(list, 1, (XENACPI_LEVELBUF_SIZE - 1), fs);
    if ( length == 0 )
    {
        ret = xenacpi_error(error_out, errno);
        goto close_out;
    }
    length = strlen(list);

    total = sizeof(struct xenacpi_vid_brightness_levels) +
            sizeof(uint32_t)*length;

    /* Over-allocate the list size. It is clear that there are fewer
     * digits represented in the string than there are chars so our
     * buffer is certainly sufficient.
     */
    levels = (struct xenacpi_vid_brightness_levels*)malloc(total);
    if ( levels == NULL )
    {
        ret = xenacpi_error(error_out, ENOMEM);
        goto close_out;
    }
    memset(levels, 0, total);

    for ( i = 0, nptr = list; i < length; i++ )
    {
        levels->levels[i] = strtol(nptr, &eptr, 10);
        if ( eptr == nptr ) /* no more digits in string */
            break;
        levels->level_count++;
        nptr = eptr;
    }

    *levels_out = levels;
    ret = 0;

close_out:
    if ( fs != NULL )
        fclose(fs);
    if ( list != NULL )
        free(list);
    return ret;
}

EXTERNAL
int xenacpi_vid_brightness_switch(int disable, int *error_out)
{
    FILE *fs;
    int ret, err = 0;

    fs = fopen("/sys/module/video/parameters/brightness_switch_enabled", "w");
    if ( fs == NULL )
        return xenacpi_error(error_out, errno);

    if ( disable )
        ret = fwrite("N", 1, 1, fs);
    else
        ret = fwrite("Y", 1, 1, fs);

    if ( ret != 1 )
        err = errno;

    fclose(fs);

    if ( err != 0 )
        return xenacpi_error(error_out, err);

    return 0;
}

