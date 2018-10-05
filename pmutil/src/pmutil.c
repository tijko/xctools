/*
 * pmiutil.c
 *
 * XenClient platform management utility main file
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

static char rcsid[] = "$Id:$";

/*
 * $Log:$
 */

#include "project.h"
#include <fcntl.h>
#include <sys/ioctl.h>
#include "pmutil.h"

/* hidraw interface */
#define HID_MAX_DESCRIPTOR_SIZE		4096

struct hidraw_report_descriptor {
	uint32_t size;
	uint8_t  value[HID_MAX_DESCRIPTOR_SIZE];
};

#define HIDIOCGRDESCSIZE	_IOR('H', 0x01, int)
#define HIDIOCGRDESC		_IOR('H', 0x02, struct hidraw_report_descriptor)

#define HIDRAW_DEVICE_NAME "/dev/hidraw%d"
#define HIDRAW_DESC_FILE   "./hidraw%d.bin"

static void hidraw_report_descriptor(int hidrawN)
{
    struct hidraw_report_descriptor report;
    char hidrawd[64];
    char descfile[64];
    int size, fd, r;

    sprintf(hidrawd, HIDRAW_DEVICE_NAME, hidrawN);
    sprintf(descfile, HIDRAW_DESC_FILE, hidrawN);

    fd = open(hidrawd, O_RDWR);
    if ( fd < 0 )
    {
        printf("Could not open %s - errno: %d\n", hidrawd, errno);
        return;
    }

    /* Get report descriptor size */
	r = ioctl(fd, HIDIOCGRDESCSIZE, &size);
    if ( r < 0 )
    {
        printf("Could not get report descriptor size for %s - error: %d\n", hidrawd, r);
        close(fd);
        return;
    }
    
    /* Get report descriptor */
    report.size = size;
    r = ioctl(fd, HIDIOCGRDESC, &report);
    if ( r < 0 )
    {
        printf("Could not get report descriptor for %s - error: %d\n", hidrawd, r);
        close(fd);
        return;
    }
    close(fd);

    fd = fopen(descfile, "wb");
    if ( fd == NULL )
    {
        printf("Open output file %s failed - errno: %d\n", descfile, errno);
        return;
    }

    r = fwrite(report.value, 1, report.size, fd);
    if ( r != report.size )
    {
        printf("Failed to write hidraw data to file %s - error: %d!\n", descfile, ferror(fd));
        close(fd);
        return;
    }

    close(fd);
    printf("hidraw data for %s written to file %s\n", hidrawd, descfile);
}

static struct option long_options[] = {
    {"bcl", required_argument, 0, 'b'},
    {"brightness", no_argument, 0, 'B'},
    {"dmar", optional_argument, 0, 'd'},
    {"invoke", required_argument, 0, 'i'},
    {"backlight", required_argument, 0, 'l'},
    {"mof", required_argument, 0, 'm'},
    {"rdesc", required_argument, 0, 'r'},
    {"wmi", no_argument, 0, 'w'},
    {"help", no_argument, 0, 'h'},
    {0, 0, 0, 0}
};

static void usage(void)
{
    printf("Usage:\n");
    printf("-b, --bcl <on|off>    turn BCL firmware control on or off\n");
    printf("-B, --brightness      trace out the brightness levels reported by firmware control\n");
    printf("-d, --dmar [file]     trace contents of ACPI DMAR file or BIOS table to stdout\n");
    printf("-i, --invoke <file>   invoke WMI method using data in input file\n");
    printf("-l, --backlight <+|-> increase/decrease brightness with software assistance\n");
    printf("-m, --mof <wmiid>     write out MOF data block for the given WMI device\n");
    printf("-r, --rdesc <hidrawN> write out USB HID raw Report Descriptor for the hidraw device number\n");
    printf("-w, --wmi             list WMI devices and their guid blocks\n");
    printf("-h, --help            prints this message\n");
}

int main(int argc, char *argv[])
{
    int c;
    int option_index = 0;

    if ( argc <= 1 )
    {
        usage();
        return;
    }

    for ( ; ; )
    {
        c = getopt_long(argc, argv, "b:Bd::i:l:m:r:wh", long_options, &option_index);
        if ( c == -1 )
            break;
        switch ( c )
        {
        case 'b':
            if ( strnicmp(optarg, "on", 2) == 0 )
                bcl_control(0);
            else if ( strnicmp(optarg, "off", 3) == 0 )
                bcl_control(1);
            else
                usage();
            break;
        case 'B':
            bcl_list_levels();
            break;
        case 'd':
            dmar_trace(optarg);
            break;
        case 'i':
            wmi_invoke(optarg);
            break;
        case 'l':
            if ( strncmp(optarg, "+", 1) == 0 )
                bcl_adjust_brightness(1);
            else if ( strncmp(optarg, "-", 1) == 0 )
                bcl_adjust_brightness(0);
            else
                usage();
            break;
        case 'm':
            wmi_write_mof((uint32_t)strtol(optarg, NULL, 10));
            break;
        case 'r':
            hidraw_report_descriptor((int)strtol(optarg, NULL, 10));
            break;
        case 'w':
            wmi_list_devices();
            break;
        case 'h':
            usage();
            break;
        case '?':
            usage();
            break;
        default:
            abort();
        }
    }

    return 0;
}

