/*
 * acpi-decode.c
 *
 * PM utility ACPI decode routines
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
#include "pmutil.h"

#define ACPI_ERROR "ACPI-DECODE-ERROR"

#define SCAN_ROM_BIOS_BASE 0xE0000
#define SCAN_ROM_BIOS_SIZE 0x20000

/* Lengths */
#define ACPI_RSDP_LENGTH      0x24
#define ACPI_RSDP_CS_LENGTH   0x14
#define ACPI_RSDP_XCS_LENGTH  0x24
#define ACPI_HEADER_LENGTH    0x24

/* Offsets */
#define ACPI_RSDP_SIGNATURE      0x00 /* 8 BYTES ASCII "RSD PTR " anchor string */
#define ACPI_RSDP_CHECKSUM       0x08 /* BYTE ACPI 1.0 CS sums to zero when added to bytes in RSDP */
#define ACPI_RSDP_OEM_ID         0x09 /* 6 BYTES ASCII OEM ID */
#define ACPI_RSDP_REVISION       0x0F /* BYTE 0 for ACPI 1.0 or 2 for ACPI 2.0 */
#define ACPI_RSDP_RSDT_BASE      0x10 /* 4 BYTES 32b physical base address of the RSDT */
#define ACPI_RSDP_XSDT_LENGTH    0x14 /* 4 BYTES length of XSDT */
#define ACPI_RSDP_XSDT_BASE      0x18 /* 8 BYTES 64b physical base address of the XSDT */
#define ACPI_RSDP_EXT_CHECKSUM   0x20 /* BYTE ACPI 2.0 CS sums to zero when added to bytes in RSDP */
#define ACPI_RSDP_RESERVED       0x21 /* 3 BYTES align table */

#define ACPI_TABLE_SIGNATURE        0x00 /* 4 BYTES signature string */
#define ACPI_TABLE_LENGTH           0x04 /* 4 BYTES length of the table in bytes including header */
#define ACPI_TABLE_REVISION         0x08 /* BYTE minor rev number */
#define ACPI_TABLE_CHECKSUM         0x09 /* BYTE sums to zero when added to bytes in table */
#define ACPI_TABLE_OEM_ID           0x0A /* 6 BYTES ASCII OEM ID */
#define ACPI_TABLE_OEM_TABLE_ID     0x10 /* 8 BYTES ASCII OEM TABLE ID */
#define ACPI_TABLE_OEM_REVISION     0x18 /* 4 BYTES OEM rev number */
#define ACPI_TABLE_CREATOR_ID       0x1C /* 4 BYTES ASCII CREATOR ID */
#define ACPI_TABLE_CREATOR_REVISION 0x20 /* 4 BYTES CREATOR REVISION */

struct acpi_rsdp_info {
    size_t rsdt_phys_addr;
    size_t xsdt_phys_addr;
    uint8_t *rsdt_addr;
    uint32_t rsdt_length;
    uint8_t *xsdt_addr;
    uint32_t xsdt_length;
    uint32_t is_rev1;
};

struct acpi_table_info {
    size_t phys_addr;
    uint8_t *addr;
    uint32_t length;
};

static int process_acpi_rsdp(struct acpi_rsdp_info *rsdpinfo, uint8_t *rsdp)
{
#define ADDR_CHECK(a, p) if (a == NULL) { \
        fprintf(stderr, "%s: failed to map ACPI table at phys="UINT_FMT"\n", ACPI_ERROR, p); \
        rc = -1; break;}
    uint8_t cs;
    uint32_t count, length;
    uint8_t *addr;
    int rc = 0;

    do
    {
        /* checksum sanity check over the RSDP */
        if ( rsdp[ACPI_RSDP_REVISION] < 2 )
        {
    	    length = ACPI_RSDP_CS_LENGTH;
    	    rsdpinfo->is_rev1 = 1;
        }
        else
    	    length = ACPI_RSDP_XCS_LENGTH;

        for ( cs = 0, count = 0; count < length; count++ )
            cs += rsdp[count];

        if ( cs != 0 )
        {
            fprintf(stderr, "%s: invalid RSDP checksum\n", ACPI_ERROR);
            rc = -1;
            break;
        }
        
        /* looks like the RSDP, get RSDP table */
        rsdpinfo->rsdt_phys_addr = (*(uint32_t*)(rsdp + ACPI_RSDP_RSDT_BASE));
        rsdpinfo->rsdt_length = ACPI_HEADER_LENGTH;
        addr = map_phys_mem(rsdpinfo->rsdt_phys_addr, ACPI_HEADER_LENGTH);
        ADDR_CHECK(addr, rsdpinfo->rsdt_phys_addr);
        
        rsdpinfo->rsdt_addr = addr;
        /* check the signatures for the RSDT */
        if ( memcmp(rsdpinfo->rsdt_addr, "RSDT", 4) != 0 )
        {
            fprintf(stderr, "%s: invalid RSDT signature=%.*s\n", ACPI_ERROR, 4, rsdpinfo->rsdt_addr);
    	    rc = -1;
            break;
        }

        /* remap the entire table */
        rsdpinfo->rsdt_length = (*(uint32_t*)(rsdpinfo->rsdt_addr + ACPI_TABLE_LENGTH));
        unmap_phys_mem(rsdpinfo->rsdt_addr, ACPI_HEADER_LENGTH);
        rsdpinfo->rsdt_addr = NULL;
        addr = map_phys_mem(rsdpinfo->rsdt_phys_addr, rsdpinfo->rsdt_length);
        ADDR_CHECK(addr, rsdpinfo->rsdt_phys_addr);
        rsdpinfo->rsdt_addr = addr;

        if ( rsdpinfo->is_rev1 )
            break;

        /* Also have an XSDT */
        rsdpinfo->xsdt_phys_addr = (*(uint64_t*)(rsdp + ACPI_RSDP_XSDT_BASE));
        rsdpinfo->xsdt_length = ACPI_HEADER_LENGTH;
        addr = map_phys_mem(rsdpinfo->xsdt_phys_addr, ACPI_HEADER_LENGTH);
        ADDR_CHECK(addr, rsdpinfo->xsdt_phys_addr);
        rsdpinfo->xsdt_addr = addr;
        
        /* check the signatures for the XSDT */
        if ( memcmp(rsdpinfo->xsdt_addr, "XSDT", 4) != 0 )
        {
            fprintf(stderr, "%s: invalid XSDT signature=%.*s\n", ACPI_ERROR, 4, rsdpinfo->xsdt_addr);
            rc = -1;
            break;
        }

        /* remap the entire table */
        rsdpinfo->xsdt_length = (*(uint32_t*)(rsdpinfo->xsdt_addr + ACPI_TABLE_LENGTH));
        unmap_phys_mem(rsdpinfo->xsdt_addr, ACPI_HEADER_LENGTH);
        rsdpinfo->xsdt_addr = NULL;
        addr = map_phys_mem(rsdpinfo->xsdt_phys_addr, rsdpinfo->xsdt_length);
        ADDR_CHECK(addr, rsdpinfo->xsdt_phys_addr);            
        rsdpinfo->xsdt_addr = addr;      
    } while (0);

#undef ADDR_CHECK
    if ( rc != 0 )
    {
        if ( rsdpinfo->rsdt_addr != NULL )
            unmap_phys_mem(rsdpinfo->rsdt_addr, rsdpinfo->rsdt_length);
        if ( rsdpinfo->xsdt_addr != NULL )
            unmap_phys_mem(rsdpinfo->xsdt_addr, rsdpinfo->xsdt_length);
    }

    return rc;
}

static int locate_acpi_pointers(struct acpi_rsdp_info *rsdpinfo)
{
    size_t loc = 0;
    uint8_t *addr;
    int rc = -1; /* in case we don't find it */

    memset(rsdpinfo, 0, sizeof(struct acpi_rsdp_info));

    /* use EFI tables if present */
    rc = efi_locate_entry("ACPI20", 6, &loc);
    if ( (rc == 0) && (loc != 0) )
    {
        addr = map_phys_mem(loc, ACPI_RSDP_LENGTH);
        if ( addr == NULL )
        {
            fprintf(stderr, "%s: failed to map EFI RSDP at phys="UINT_FMT"\n", ACPI_ERROR, loc);
            return -1;
        }
        rc = process_acpi_rsdp(rsdpinfo, addr);
        unmap_phys_mem(addr, ACPI_RSDP_LENGTH);
        return rc;
    }

    /* locate ACPI entry via memory scan of ROM region */
    addr = map_phys_mem(SCAN_ROM_BIOS_BASE, SCAN_ROM_BIOS_SIZE);
    if ( addr == NULL )
    {
        fprintf(stderr, "%s: failed to map ROM BIOS at phys=%x\n", ACPI_ERROR, SCAN_ROM_BIOS_BASE);
        return -1;
    }

    for ( loc = 0; loc <= (SCAN_ROM_BIOS_SIZE - ACPI_RSDP_LENGTH); loc += 16 ) /* stop before 0xFFDC */
    {
        /* look for RSD PTR  signature */
        if ( memcmp(addr + loc, "RSD PTR ", 8 ) == 0)
        {
            rc = process_acpi_rsdp(rsdpinfo, addr + loc);
            if ( rc == 0 ) /* found it */
                break;
        }
    }
    unmap_phys_mem(addr, SCAN_ROM_BIOS_SIZE);

    return rc;
}

static int locate_rsdt_acpi_table(struct acpi_rsdp_info *rsdpinfo, const char *signature, struct acpi_table_info *ti)
{
    uint32_t *addr_list;
    uint32_t length, count, i;
    int rc = -1;
    uint8_t *addr;

    if ( rsdpinfo->rsdt_length <= ACPI_TABLE_LENGTH )
    {
        fprintf(stderr, "%s: invalid RSDT - no table pointers\n", ACPI_ERROR);
        return -1; /* invalid - no tables?? */
    }

    length = rsdpinfo->rsdt_length - ACPI_HEADER_LENGTH;
    count = length/sizeof(uint32_t);
    addr_list = (uint32_t*)(rsdpinfo->rsdt_addr + ACPI_HEADER_LENGTH);

    for ( i = 0; i < count; i++, addr_list++ )
    {
        addr = map_phys_mem(*addr_list, ACPI_HEADER_LENGTH);
        if ( addr == NULL )
            continue;
       
        if ( memcmp(addr, signature, 4) == 0 )
        {
            ti->length = (*(uint32_t*)(addr + ACPI_TABLE_LENGTH));
            unmap_phys_mem(addr, ACPI_HEADER_LENGTH);
            addr = map_phys_mem(*addr_list, ti->length);
            if ( addr == NULL )
            {
                fprintf(stderr, "%s: failed to map table #%d at phys=%x\n", ACPI_ERROR, i, *addr_list);
                break;
            }
            ti->phys_addr = *addr_list;
            ti->addr = addr;
            rc = 0;
            break;
        }
        
        unmap_phys_mem(addr, ACPI_HEADER_LENGTH);
    }

    return rc;
}

static int locate_xsdt_acpi_table(struct acpi_rsdp_info *rsdpinfo, const char *signature, struct acpi_table_info *ti)
{
    uint64_t *addr_list;
    uint32_t length, count, i;
    int rc = -1;
    uint8_t *addr;

    if ( rsdpinfo->xsdt_length <= ACPI_TABLE_LENGTH )
    {
        fprintf(stderr, "%s: invalid XSDT - no table pointers\n", ACPI_ERROR);
        return -1; /* invalid - no tables?? */
    }

    length = rsdpinfo->xsdt_length - ACPI_HEADER_LENGTH;
    count = length/sizeof(uint64_t);
    addr_list = (uint64_t*)(rsdpinfo->xsdt_addr + ACPI_HEADER_LENGTH);

    for ( i = 0; i < count; i++, addr_list++ )
    {
        addr = map_phys_mem(*addr_list, ACPI_HEADER_LENGTH);
        if ( addr == NULL )
            continue;
       
        if ( memcmp(addr, signature, 4) == 0 )
        {            
            ti->length = (*(uint32_t*)(addr + ACPI_TABLE_LENGTH));
            unmap_phys_mem(addr, ACPI_HEADER_LENGTH);
            addr = map_phys_mem(*addr_list, ti->length);
            if ( addr == NULL )
            {
                fprintf(stderr, "%s: failed to map table #%d at phys=%lx\n", ACPI_ERROR, i, (long unsigned int)*addr_list);
                break;
            }
            ti->phys_addr = *addr_list;
            ti->addr = addr;
            rc = 0;
            break;
        }
        
        unmap_phys_mem(addr, ACPI_HEADER_LENGTH);
    }

    return rc;
}

static int decode_acpi_tables(struct acpi_rsdp_info *rsdpinfo, const char *table_name, uint8_t *table_buf, uint32_t length)
{
    uint8_t *ptr;
    int rc = 0;
    struct acpi_table_info ti = {0};

    /* locate the requested table - use XSDT if present */
    if ( !rsdpinfo->is_rev1 )
    {
        rc = locate_xsdt_acpi_table(rsdpinfo, table_name, &ti);
        ptr = rsdpinfo->xsdt_addr;
    }
    else
    {
        rc = locate_rsdt_acpi_table(rsdpinfo, table_name, &ti);
        ptr = rsdpinfo->rsdt_addr;
    }
    
    if ( rc != 0 )
    {
        fprintf(stderr, "%s: failed to locate %s table\n", ACPI_ERROR, table_name);
        return rc;
    }
    
    if ( ti.length <= length )
    {
        /* If we got it, copy it over */
        memcpy(table_buf, ti.addr, ti.length);
    }
    else
    {
        fprintf(stderr, "%s: cannot copy %s table length=%x - out of space\n", ACPI_ERROR, table_name, ti.length);
        rc = -1;
    }
    
    unmap_phys_mem(ti.addr, ti.length);
    return rc;
}

int acpi_get_table(const char *table_name, uint8_t *table_buf, uint32_t length)
{
    struct acpi_rsdp_info rsdpinfo;
    int rc;

    /* first locate the ACPI tables */
    rc = locate_acpi_pointers(&rsdpinfo);
    if ( rc != 0 )
    {
        fprintf(stderr, "%s: failed to find ACPI info\n", ACPI_ERROR);
        return rc;
    }

    /* process the ACPI tables */
    rc = decode_acpi_tables(&rsdpinfo, table_name, table_buf, length);

    /* cleanup */
    unmap_phys_mem(rsdpinfo.rsdt_addr, rsdpinfo.rsdt_length);
    if ( !rsdpinfo.is_rev1 )
    	unmap_phys_mem(rsdpinfo.xsdt_addr, rsdpinfo.xsdt_length);

    if ( rc != 0 )
        fprintf(stderr, "%s: decoding failed\n", ACPI_ERROR);

    return rc;
}

