/*
 * pmutil.h
 *
 * XenClient platform management utility main header
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

#ifndef __PMUTIL_H__
#define __PMUTIL_H__

#if __WORDSIZE == 64
#define INT_FMT "%ld"
#define UINT_FMT "%lx"
#else
#define INT_FMT "%d"
#define UINT_FMT "%x"
#endif 

#define SURFMAN_SERVICE "com.citrix.xenclient.surfman"
#define SURFMAN_PATH    "/"
#define XCPMD_SERVICE   "com.citrix.xenclient.xcpmd"
#define XCPMD_PATH      "/"

#define ACPI_SIG_DMAR      "DMAR" /* DMA Remapping table */
#define ACPI_SIG_XEN_DMAR  "\x00MAR" /* Xen modified DMA Remapping table */

int acpi_get_table(const char *table_name, uint8_t *table_buf, uint32_t length);

void dmar_trace(char *dmar_file);
void bcl_adjust_brightness(int increase);
void bcl_control(int disable);
void bcl_list_levels(void);
void wmi_list_devices(void);
void wmi_write_mof(uint32_t wmiid);
void wmi_invoke(char *wmi_file);

/* utils */
int strnicmp(const char *s1, const char *s2, size_t len);
int file_set_blocking(int fd);
int file_set_nonblocking(int fd);
uint8_t *map_phys_mem(size_t phys_addr, size_t length);
void unmap_phys_mem(uint8_t *addr, size_t length);
int efi_locate_entry(const char *efi_entry, uint32_t length, size_t *location);
char* xenstore_read_str(struct xs_handle *xs, char *path);
unsigned int xenstore_read_uint(struct xs_handle *xs, char *path);

#endif /* __PMUTIL_H__ */

