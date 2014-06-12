/*
 * amlres.c
 *
 * XEN ACPI AML resource building code.
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

#ifndef XENAML_TEST_APP
#include "project.h"
#else
#include "project_test.h"
#endif
#define ACPI_MACHINE_WIDTH 32 /* not really using this */
#include "actypes.h"
#include "actbl.h"
#include "amlcode.h"
#include "amldefs.h"

#define XENAML_ADDR_SIZE_QWORD 0x01
#define XENAML_ADDR_SIZE_DWORD 0x02
#define XENAML_ADDR_SIZE_WORD  0x03

#define XENAML_RESOURCE_TYPE_MEMORY_RANGE 0x00
#define XENAML_RESOURCE_TYPE_IO_RANGE     0x01
#define XENAML_RESOURCE_TYPE_BUS_RANGE    0x02

#define XENAML_SMALL_RES_TAG(n, l) \
    (uint8_t)((0x78 & (n << 3)) | (l & 0x07))

#define XENAML_SMALL_RES_TAG_SIZE 0x1

#define XENAML_SMALL_RES_IRQ           0x4
#define XENAML_SMALL_RES_DMA           0x5
#define XENAML_SMALL_RES_START_DEP     0x6
#define XENAML_SMALL_RES_END_DEP       0x7
#define XENAML_SMALL_RES_IO            0x8
#define XENAML_SMALL_RES_FIXED_IO      0x9
#define XENAML_SMALL_RES_VENDOR        0xE
#define XENAML_SMALL_RES_END           0xF

#define XENAML_LARGE_RES_TAG_NAME(n) \
    (uint8_t)((1 << 7) | (n & 0x7F))

#define XENAML_LARGE_RES_TAG_SIZE 0x3

#define XENAML_LARGE_RES_MEMORY32            0x5
#define XENAML_LARGE_RES_MEMORY32_FIXED      0x6
#define XENAML_LARGE_RES_QWORD_ADDR_SPACE    0xA
#define XENAML_LARGE_RES_DWORD_ADDR_SPACE    0x7
#define XENAML_LARGE_RES_WORD_ADDR_SPACE     0x8
#define XENAML_LARGE_RES_EXTENDED_ADDR_SPACE 0xB
#define XENAML_LARGE_RES_INTERRUPT           0x9
#define XENAML_LARGE_RES_REGISTER            0x2

#define XENAML_QWORD_TO_DWORD(q) ((uint32_t)(0x00000000FFFFFFFF & q))
#define XENAML_QWORD_TO_WORD(q)  ((uint16_t)(0x000000000000FFFF & q))

static void*
xenaml_resource_node(uint32_t length, void *pma)
{
    struct xenaml_node *nnode;

    nnode = xenaml_alloc_node(pma, length, 1);
    if ( nnode == NULL )
        return NULL;

    nnode->op = XENAML_UNASSIGNED_OPCODE;
    nnode->flags = XENAML_FLAG_RESOURCE;

    return nnode;
}

static xenaml_bool
xenaml_encode_bits(uint8_t *enc_buf,
                   uint8_t enc_bytes,
                   uint8_t *vals,
                   uint8_t count)
{
    uint8_t i;

    for ( i = 0; i < count; i++ )
    {
        if ( (vals[i] >= 0)&&(vals[i] <= 7) )
            enc_buf[0] |= (1 << vals[i]);
        else if ( (enc_bytes > 1)&&(vals[i] >= 8)&&(vals[i] <= 15) )
            enc_buf[1] |= (1 << (vals[i] - 8));
        else
            return 0;
    }

    return 1;
}

static void
xenaml_large_res_header(struct xenaml_node *rnode,
                        uint8_t name,
                        uint16_t length)
{
    uint8_t *ptr = &rnode->buffer[1];

    rnode->buffer[0] = XENAML_LARGE_RES_TAG_NAME(name);
    xenaml_write_word(&ptr, length);
}

static xenaml_bool
xenaml_io_type_flags_check(enum xenaml_isa_ranges isa_ranges,
                           enum xenaml_translation_type translation_type,
                           enum xenaml_translation_density translation_density,
                           uint8_t *flags_out)
{
    if ( (isa_ranges >= XENAML_ISA_RANGE_MAX)||
         (translation_type >= XENAML_TRANSLATION_TYPE_MAX)||
         (translation_density >= XENAML_TRANSLATION_DENSITY_MAX) )
        return 0;

    *flags_out = 0;
    *flags_out |= (0x20 & (translation_density << 5)) |
                  (0x10 & (translation_type << 4)) |
                  (0x03 & isa_ranges);

    return 1;
}

static xenaml_bool
xenaml_memory_type_flags_check(enum xenaml_memory_caching cacheable,
                               xenaml_bool read_write,
                               enum xenaml_memory_type memory_type,
                               enum xenaml_translation_type translation_type,
                               uint8_t *flags_out)
{
    if ( (cacheable >= XENAML_MEMORY_CACHING_MAX)||
         (memory_type >= XENAML_MEMORY_TYPE_MAX)||
         (translation_type >= XENAML_TRANSLATION_TYPE_MAX) )
        return 0;

    *flags_out = 0;
    *flags_out |= (0x20 & (translation_type << 5)) |
                  (0x18 & (memory_type << 3)) |
                  (0x06 & (cacheable << 1));
    *flags_out |= (read_write) ? 1 : 0;

    return 1;
}

static xenaml_bool
xenaml_validate_address_values(xenaml_bool is_min_fixed,
                               xenaml_bool is_max_fixed,
                               struct xenaml_address_values_args *values)
{
    uint64_t gplus1 = values->address_granularity + 1;

    /* So the basic idea is that no matter the address space precision
     * of the resource being created, all the address values are stored
     * as 64b integers are validated as such. Thus word values will
     * all me within the lower 4 bytes but relative to eachother. This way
     * a single implementation can be used. See the APCI 3.0 spec
     * section 6.4.3.5 and the table there for the details.
     */
    if ( values->address_maximum < values->address_minimum )
        return 0; /* illegal, handles case where they are both 0 */

    if ( values->range_length == 0 )
    {
        if ( (is_min_fixed)&&(is_max_fixed) )
            return 0; /* illegal */

        /* Variable size, variable location resource descriptor for _PRS */
        if ( (is_min_fixed)&&(values->address_minimum % gplus1) != 0 )
            return 0; /* illegal */

        if ( (is_min_fixed)&&((values->address_maximum + 1) % gplus1) != 0 )
            return 0; /* illegal */

        return 1;
    }

    if ( (!is_min_fixed)&&(!is_max_fixed) )
    {
        /* Fixed size, variable location resource descriptor for _PRS. */
        if ( (values->range_length % gplus1) != 0 )
            return 0; /* illegal */

        return 1;
    }

    if ( (is_min_fixed)&&(is_max_fixed) )
    {
        /* Fixed size, fixed location resource descriptor for _PRS. */
        if ( values->address_granularity != 0 )
            return 0; /* illegal */
        if ( (values->address_maximum - values->address_minimum + 1) !=
              values->range_length )
            return 0; /* illegal */

        return 1;
    }

    /* Other combinations are illegal */
    return 0;
}

static uint8_t
xenaml_general_flags(struct xenaml_address_space_common_args *common)
{
    uint8_t flags = 0;

    flags |= (common->is_max_fixed) ? (1 << 3) : 0;
    flags |= (common->is_min_fixed) ? (1 << 2) : 0;
    flags |= (0x02 & (common->decode << 1))|(0x01 & common->resource_usage);

    return flags;
}

static uint16_t
xenaml_find_space_desc_length(struct xenaml_address_space_source_args *source,
                              uint16_t fixed_length)
{
    uint16_t length = fixed_length;

    /* If present, add the source string length + 2 for the index and null bytes */
    if ( (source != NULL)&&(source->present)&&
         (source->resource_source != NULL) )
        length += (uint16_t)(2 + strlen(source->resource_source));

    return length;
}

static void
xenaml_write_qword_address_values(uint8_t **buffer,
                                  struct xenaml_address_values_args *values)
{
    xenaml_write_qword(buffer, values->address_granularity);
    xenaml_write_qword(buffer, values->address_minimum);
    xenaml_write_qword(buffer, values->address_maximum);
    xenaml_write_qword(buffer, values->address_translation);
    xenaml_write_qword(buffer, values->range_length);
}

static void
xenaml_write_dword_address_values(uint8_t **buffer,
                                  struct xenaml_address_values_args *values)
{
    xenaml_write_dword(buffer,
        XENAML_QWORD_TO_DWORD(values->address_granularity));
    xenaml_write_dword(buffer,
        XENAML_QWORD_TO_DWORD(values->address_minimum));
    xenaml_write_dword(buffer,
        XENAML_QWORD_TO_DWORD(values->address_maximum));
    xenaml_write_dword(buffer,
        XENAML_QWORD_TO_DWORD(values->address_translation));
    xenaml_write_dword(buffer,
        XENAML_QWORD_TO_DWORD(values->range_length));
}

static void
xenaml_write_word_address_values(uint8_t **buffer,
                                 struct xenaml_address_values_args *values)
{
    xenaml_write_word(buffer,
        XENAML_QWORD_TO_WORD(values->address_granularity));
    xenaml_write_word(buffer,
        XENAML_QWORD_TO_WORD(values->address_minimum));
    xenaml_write_word(buffer,
        XENAML_QWORD_TO_WORD(values->address_maximum));
    xenaml_write_word(buffer,
        XENAML_QWORD_TO_WORD(values->address_translation));
    xenaml_write_word(buffer,
        XENAML_QWORD_TO_WORD(values->range_length));
}

static void
xenaml_write_source_values(uint8_t *ptr,
                           struct xenaml_address_space_source_args *source)
{
    size_t length;

    if ( (source == NULL)||(!source->present)||
         (source->resource_source == NULL) )
        return;

    length = strlen(source->resource_source);
    *ptr = source->resource_source_index;
    memcpy(++ptr, source->resource_source, length);
    *(ptr + length) = '\0';
}

EXTERNAL void*
xenaml_irq(enum xenaml_irq_mode edge_level,
           enum xenaml_irq_active active_level,
           xenaml_bool shared,
           uint8_t *irqs,
           uint8_t count,
           void *pma)
{
    struct xenaml_node *rnode;
    uint8_t b2[2] = {0};

    if ( (edge_level >= XENAML_IRQ_MODE_MAX)||
         (active_level >= XENAML_IRQ_ACTIVE_MAX)||
         (irqs == NULL)||(count < 1)||(count >= 16) )
        return NULL;

    if ( !xenaml_encode_bits(&b2[0], 2, irqs, count) )
        return NULL;

    rnode = xenaml_resource_node(0x3 + XENAML_SMALL_RES_TAG_SIZE, pma);
    if ( rnode == NULL )
        return NULL;
    rnode->buffer[0] = XENAML_SMALL_RES_TAG(XENAML_SMALL_RES_IRQ, 0x3);
    memcpy(&rnode->buffer[1], &b2[0], 2);
    rnode->buffer[3] = (shared) ? (1 << 4) : 0;
    rnode->buffer[3] |= (0x08 & (active_level << 3))|(0x01 & edge_level);

    return rnode;
}

EXTERNAL void*
xenaml_irq_noflags(uint8_t *irqs,
                   uint8_t count,
                   void *pma)
{
    struct xenaml_node *rnode;
    uint8_t b2[2] = {0};

    if ( (irqs == NULL)||(count < 1)||(count >= 16) )
        return NULL;

    if ( !xenaml_encode_bits(&b2[0], 2, irqs, count) )
        return NULL;

    rnode = xenaml_resource_node(0x2 + XENAML_SMALL_RES_TAG_SIZE, pma);
    if ( rnode == NULL )
        return NULL;
    rnode->buffer[0] = XENAML_SMALL_RES_TAG(XENAML_SMALL_RES_IRQ, 0x2);
    memcpy(&rnode->buffer[1], &b2[0], 2);

    return rnode;
}

EXTERNAL void*
xenaml_dma(enum xenaml_dma_type dma_type,
           enum xenaml_dma_transfer_size dma_transfer_size,
           xenaml_bool bus_master,
           uint8_t *channels,
           uint8_t count,
           void *pma)
{
    struct xenaml_node *rnode;
    uint8_t b1 = 0;

    if ( (dma_type >= XENAML_DMA_TYPE_MAX)||
         (dma_transfer_size >= XENAML_DMA_TRANSER_SIZE_MAX)||
         (channels == NULL)||(count < 1)||(count >= 8) )
        return NULL;

    if ( !xenaml_encode_bits(&b1, 1, channels, count) )
        return NULL;

    rnode = xenaml_resource_node(0x2 + XENAML_SMALL_RES_TAG_SIZE, pma);
    if ( rnode == NULL )
        return NULL;
    rnode->buffer[0] = XENAML_SMALL_RES_TAG(XENAML_SMALL_RES_DMA, 0x2);
    rnode->buffer[1] = b1;
    rnode->buffer[2] = (bus_master) ? (1 << 2) : 0;
    rnode->buffer[2] |= (0x60 & (dma_type << 5))|(0x03 & dma_transfer_size);

    return rnode;
}

EXTERNAL void*
xenaml_start_dependent_fn(enum xenaml_dep_priority capability_priority,
                          enum xenaml_dep_priority performance_priority,
                          void *pma)
{
    struct xenaml_node *rnode;

    if ( (capability_priority >= XENAML_DEP_PRIORITY_MAX)||
         (performance_priority >= XENAML_DEP_PRIORITY_MAX) )
        return NULL;

    rnode = xenaml_resource_node(0x1 + XENAML_SMALL_RES_TAG_SIZE, pma);
    if ( rnode == NULL )
        return NULL;
    rnode->buffer[0] = XENAML_SMALL_RES_TAG(XENAML_SMALL_RES_START_DEP, 0x1);
    rnode->buffer[1] = (0x0C & (performance_priority << 2))|
                       (0x03 & capability_priority);

    return rnode;
}

EXTERNAL void*
xenaml_start_dependent_fn_nopri(void *pma)
{
    struct xenaml_node *rnode;

    rnode = xenaml_resource_node(0x0 + XENAML_SMALL_RES_TAG_SIZE, pma);
    if ( rnode == NULL )
        return NULL;
    rnode->buffer[0] = XENAML_SMALL_RES_TAG(XENAML_SMALL_RES_START_DEP, 0x0);

    return rnode;
}

EXTERNAL void*
xenaml_end_dependent_fn(void *pma)
{
    struct xenaml_node *rnode;

    rnode = xenaml_resource_node(0x0 + XENAML_SMALL_RES_TAG_SIZE, pma);
    if ( rnode == NULL )
        return NULL;
    rnode->buffer[0] = XENAML_SMALL_RES_TAG(XENAML_SMALL_RES_END_DEP, 0x0);

    return rnode;
}

EXTERNAL void*
xenaml_io(enum xenaml_io_decode decode,
          uint16_t addresss_minimum,
          uint16_t address_maximum,
          uint8_t address_alignment,
          uint8_t range_length,
          void *pma)
{
    struct xenaml_node *rnode;
    uint8_t *ptr;

    if ( decode >= XENAML_IO_DECODE_MAX )
        return NULL;

    rnode = xenaml_resource_node(0x7 + XENAML_SMALL_RES_TAG_SIZE, pma);
    if ( rnode == NULL )
        return NULL;
    rnode->buffer[0] = XENAML_SMALL_RES_TAG(XENAML_SMALL_RES_IO, 0x7);
    rnode->buffer[1] = (0x03 & decode);
    ptr = &rnode->buffer[2];
    xenaml_write_word(&ptr, addresss_minimum);
    xenaml_write_word(&ptr, address_maximum);
    xenaml_write_byte(&ptr, address_alignment);
    xenaml_write_byte(&ptr, range_length);

    return rnode;
}

EXTERNAL void*
xenaml_fixed_io(uint16_t address_base,
                uint8_t range_length,
                void *pma)
{
    struct xenaml_node *rnode;
    uint8_t *ptr;

    rnode = xenaml_resource_node(0x3 + XENAML_SMALL_RES_TAG_SIZE, pma);
    if ( rnode == NULL )
        return NULL;
    rnode->buffer[0] = XENAML_SMALL_RES_TAG(XENAML_SMALL_RES_FIXED_IO, 0x3);
    ptr = &rnode->buffer[1];
    xenaml_write_word(&ptr, address_base);
    xenaml_write_byte(&ptr, range_length);

    return rnode;
}

EXTERNAL void*
xenaml_vendor_short(uint8_t *vendor_bytes,
                    uint8_t count,
                    void *pma)
{
    struct xenaml_node *rnode;

    if ( (vendor_bytes == NULL)||(count < 1)||(count > 7) )
        return NULL;

    rnode = xenaml_resource_node(count + XENAML_SMALL_RES_TAG_SIZE, pma);
    if ( rnode == NULL )
        return NULL;
    rnode->buffer[0] = XENAML_SMALL_RES_TAG(XENAML_SMALL_RES_VENDOR, count);
    memcpy(&rnode->buffer[1], vendor_bytes, count);

    return rnode;
}

EXTERNAL void*
xenaml_end(void *pma)
{
    struct xenaml_node *rnode;

    rnode = xenaml_resource_node(0x1 + XENAML_SMALL_RES_TAG_SIZE, pma);
    if ( rnode == NULL )
        return NULL;
    rnode->buffer[0] = XENAML_SMALL_RES_TAG(XENAML_SMALL_RES_END, 0x1);
    /* leave checsum value 0 as the ASL compiler does */

    return rnode;
}

EXTERNAL void*
xenaml_memory32(xenaml_bool read_write,
                uint32_t address_minimum,
                uint32_t address_maximum,
                uint32_t address_alignment,
                uint32_t range_length,
                void *pma)
{
    struct xenaml_node *rnode;
    uint8_t *ptr;

    rnode = xenaml_resource_node(0x11 + XENAML_LARGE_RES_TAG_SIZE, pma);
    if ( rnode == NULL )
        return NULL;
    xenaml_large_res_header(rnode, XENAML_LARGE_RES_MEMORY32, 0x11);
    rnode->buffer[3] = (read_write) ? 1 : 0;
    ptr = &rnode->buffer[4];
    xenaml_write_dword(&ptr, address_minimum);
    xenaml_write_dword(&ptr, address_maximum);
    xenaml_write_dword(&ptr, address_alignment);
    xenaml_write_dword(&ptr, range_length);

    return rnode;
}

EXTERNAL void*
xenaml_memory32_fixed(xenaml_bool read_write,
                      uint32_t address_base,
                      uint32_t range_length,
                      void *pma)
{
    struct xenaml_node *rnode;
    uint8_t *ptr;

    rnode = xenaml_resource_node(0x9 + XENAML_LARGE_RES_TAG_SIZE, pma);
    if ( rnode == NULL )
        return NULL;
    xenaml_large_res_header(rnode, XENAML_LARGE_RES_MEMORY32_FIXED, 0x9);
    rnode->buffer[3] = (read_write) ? 1 : 0;
    ptr = &rnode->buffer[4];
    xenaml_write_dword(&ptr, address_base);
    xenaml_write_dword(&ptr, range_length);

    return rnode;
}

EXTERNAL void*
xenaml_qword_io(struct xenaml_qword_io_args *args,
                void *pma)
{
    struct xenaml_node *rnode;
    uint8_t tsf = 0;
    xenaml_bool rc;
    uint16_t length;
    uint8_t *ptr;

    rc = xenaml_io_type_flags_check(args->isa_ranges,
                                    args->translation_type,
                                    args->translation_density,
                                    &tsf);
    if ( !rc )
        return NULL;

    rc = xenaml_validate_address_values(args->common_args.is_min_fixed,
                                        args->common_args.is_max_fixed,
                                        &args->address_args);
    if ( !rc )
        return NULL;

    length = xenaml_find_space_desc_length(&args->source_args, 0x2B);

    rnode = xenaml_resource_node(length + XENAML_LARGE_RES_TAG_SIZE, pma);
    if ( rnode == NULL )
        return NULL;
    xenaml_large_res_header(rnode, XENAML_LARGE_RES_QWORD_ADDR_SPACE, length);
    rnode->buffer[3] = XENAML_RESOURCE_TYPE_IO_RANGE;
    rnode->buffer[4] = xenaml_general_flags(&args->common_args);
    rnode->buffer[5] = tsf;
    ptr = &rnode->buffer[6];
    xenaml_write_qword_address_values(&ptr, &args->address_args);
    xenaml_write_source_values(ptr, &args->source_args);

    return rnode;
}

EXTERNAL void*
xenaml_qword_memory(struct xenaml_qword_memory_args *args,
                    void *pma)
{
    struct xenaml_node *rnode;
    uint8_t tsf = 0;
    xenaml_bool rc;
    uint16_t length;
    uint8_t *ptr;

    rc = xenaml_memory_type_flags_check(args->cacheable,
                                        args->read_write,
                                        args->memory_type,
                                        args->translation_type,
                                        &tsf);
    if ( !rc )
        return NULL;

    rc = xenaml_validate_address_values(args->common_args.is_min_fixed,
                                        args->common_args.is_max_fixed,
                                        &args->address_args);
    if ( !rc )
        return NULL;

    length = xenaml_find_space_desc_length(&args->source_args, 0x2B);

    rnode = xenaml_resource_node(length + XENAML_LARGE_RES_TAG_SIZE, pma);
    if ( rnode == NULL )
        return NULL;
    xenaml_large_res_header(rnode, XENAML_LARGE_RES_QWORD_ADDR_SPACE, length);
    rnode->buffer[3] = XENAML_RESOURCE_TYPE_MEMORY_RANGE;
    rnode->buffer[4] = xenaml_general_flags(&args->common_args);
    rnode->buffer[5] = tsf;
    ptr = &rnode->buffer[6];
    xenaml_write_qword_address_values(&ptr, &args->address_args);
    xenaml_write_source_values(ptr, &args->source_args);

    return rnode;
}

EXTERNAL void*
xenaml_qword_space(struct xenaml_qword_space_args *args,
                   void *pma)
{
    struct xenaml_node *rnode;
    xenaml_bool rc;
    uint16_t length;
    uint8_t *ptr;

    if ( args->resource_type < XENAML_RESOURCE_TYPE_MIN )
        return NULL;

    rc = xenaml_validate_address_values(args->common_args.is_min_fixed,
                                        args->common_args.is_max_fixed,
                                        &args->address_args);
    if ( !rc )
        return NULL;

    length = xenaml_find_space_desc_length(&args->source_args, 0x2B);

    rnode = xenaml_resource_node(length + XENAML_LARGE_RES_TAG_SIZE, pma);
    if ( rnode == NULL )
        return NULL;
    xenaml_large_res_header(rnode, XENAML_LARGE_RES_QWORD_ADDR_SPACE, length);
    rnode->buffer[3] = args->resource_type;
    rnode->buffer[4] = xenaml_general_flags(&args->common_args);
    rnode->buffer[5] = args->type_specific_flags;
    ptr = &rnode->buffer[6];
    xenaml_write_qword_address_values(&ptr, &args->address_args);
    xenaml_write_source_values(ptr, &args->source_args);

    return rnode;
}

EXTERNAL void*
xenaml_dword_io(struct xenaml_dword_io_args *args,
                void *pma)
{
    struct xenaml_node *rnode;
    uint8_t tsf = 0;
    xenaml_bool rc;
    uint16_t length;
    uint8_t *ptr;

    rc = xenaml_io_type_flags_check(args->isa_ranges,
                                    args->translation_type,
                                    args->translation_density,
                                    &tsf);
    if ( !rc )
        return NULL;

    rc = xenaml_validate_address_values(args->common_args.is_min_fixed,
                                        args->common_args.is_max_fixed,
                                        &args->address_args);
    if ( !rc )
        return NULL;

    length = xenaml_find_space_desc_length(&args->source_args, 0x17);

    rnode = xenaml_resource_node(length + XENAML_LARGE_RES_TAG_SIZE, pma);
    if ( rnode == NULL )
        return NULL;
    xenaml_large_res_header(rnode, XENAML_LARGE_RES_DWORD_ADDR_SPACE, length);
    rnode->buffer[3] = XENAML_RESOURCE_TYPE_IO_RANGE;
    rnode->buffer[4] = xenaml_general_flags(&args->common_args);
    rnode->buffer[5] = tsf;
    ptr = &rnode->buffer[6];
    xenaml_write_dword_address_values(&ptr, &args->address_args);
    xenaml_write_source_values(ptr, &args->source_args);

    return rnode;
}

EXTERNAL void*
xenaml_dword_memory(struct xenaml_dword_memory_args *args,
                    void *pma)
{
    struct xenaml_node *rnode;
    uint8_t tsf = 0;
    xenaml_bool rc;
    uint16_t length;
    uint8_t *ptr;

    rc = xenaml_memory_type_flags_check(args->cacheable,
                                        args->read_write,
                                        args->memory_type,
                                        args->translation_type,
                                        &tsf);
    if ( !rc )
        return NULL;

    rc = xenaml_validate_address_values(args->common_args.is_min_fixed,
                                        args->common_args.is_max_fixed,
                                        &args->address_args);
    if ( !rc )
        return NULL;

    length = xenaml_find_space_desc_length(&args->source_args, 0x17);

    rnode = xenaml_resource_node(length + XENAML_LARGE_RES_TAG_SIZE, pma);
    if ( rnode == NULL )
        return NULL;
    xenaml_large_res_header(rnode, XENAML_LARGE_RES_DWORD_ADDR_SPACE, length);
    rnode->buffer[3] = XENAML_RESOURCE_TYPE_MEMORY_RANGE;
    rnode->buffer[4] = xenaml_general_flags(&args->common_args);
    rnode->buffer[5] = tsf;
    ptr = &rnode->buffer[6];
    xenaml_write_dword_address_values(&ptr, &args->address_args);
    xenaml_write_source_values(ptr, &args->source_args);

    return rnode;
}

EXTERNAL void*
xenaml_dword_space(struct xenaml_dword_space_args *args,
                   void *pma)
{
    struct xenaml_node *rnode;
    xenaml_bool rc;
    uint16_t length;
    uint8_t *ptr;

    if ( args->resource_type < XENAML_RESOURCE_TYPE_MIN )
        return NULL;

    rc = xenaml_validate_address_values(args->common_args.is_min_fixed,
                                        args->common_args.is_max_fixed,
                                        &args->address_args);
    if ( !rc )
        return NULL;

    length = xenaml_find_space_desc_length(&args->source_args, 0x17);

    rnode = xenaml_resource_node(length + XENAML_LARGE_RES_TAG_SIZE, pma);
    if ( rnode == NULL )
        return NULL;
    xenaml_large_res_header(rnode, XENAML_LARGE_RES_DWORD_ADDR_SPACE, length);
    rnode->buffer[3] = args->resource_type;
    rnode->buffer[4] = xenaml_general_flags(&args->common_args);
    rnode->buffer[5] = args->type_specific_flags;
    ptr = &rnode->buffer[6];
    xenaml_write_dword_address_values(&ptr, &args->address_args);
    xenaml_write_source_values(ptr, &args->source_args);

    return rnode;
}

EXTERNAL void*
xenaml_word_bus(struct xenaml_word_bus_args *args,
                void *pma)
{
    struct xenaml_node *rnode;
    xenaml_bool rc;
    uint16_t length;
    uint8_t *ptr;

    rc = xenaml_validate_address_values(args->common_args.is_min_fixed,
                                        args->common_args.is_max_fixed,
                                        &args->address_args);
    if ( !rc )
        return NULL;

    length = xenaml_find_space_desc_length(&args->source_args, 0x0D);

    rnode = xenaml_resource_node(length + XENAML_LARGE_RES_TAG_SIZE, pma);
    if ( rnode == NULL )
        return NULL;
    xenaml_large_res_header(rnode, XENAML_LARGE_RES_WORD_ADDR_SPACE, length);
    rnode->buffer[3] = XENAML_RESOURCE_TYPE_BUS_RANGE;
    rnode->buffer[4] = xenaml_general_flags(&args->common_args);
    rnode->buffer[5] = 0;
    ptr = &rnode->buffer[6];
    xenaml_write_word_address_values(&ptr, &args->address_args);
    xenaml_write_source_values(ptr, &args->source_args);

    return rnode;
}

EXTERNAL void*
xenaml_word_io(struct xenaml_word_io_args *args,
               void *pma)
{
    struct xenaml_node *rnode;
    uint8_t tsf = 0;
    xenaml_bool rc;
    uint16_t length;
    uint8_t *ptr;

    rc = xenaml_io_type_flags_check(args->isa_ranges,
                                    args->translation_type,
                                    args->translation_density,
                                    &tsf);
    if ( !rc )
        return NULL;

    rc = xenaml_validate_address_values(args->common_args.is_min_fixed,
                                        args->common_args.is_max_fixed,
                                        &args->address_args);
    if ( !rc )
        return NULL;

    length = xenaml_find_space_desc_length(&args->source_args, 0x0D);

    rnode = xenaml_resource_node(length + XENAML_LARGE_RES_TAG_SIZE, pma);
    if ( rnode == NULL )
        return NULL;
    xenaml_large_res_header(rnode, XENAML_LARGE_RES_WORD_ADDR_SPACE, length);
    rnode->buffer[3] = XENAML_RESOURCE_TYPE_IO_RANGE;
    rnode->buffer[4] = xenaml_general_flags(&args->common_args);
    rnode->buffer[5] = tsf;
    ptr = &rnode->buffer[6];
    xenaml_write_word_address_values(&ptr, &args->address_args);
    xenaml_write_source_values(ptr, &args->source_args);

    return rnode;
}

EXTERNAL void*
xenaml_word_space(struct xenaml_word_space_args *args,
                  void *pma)
{
    struct xenaml_node *rnode;
    xenaml_bool rc;
    uint16_t length;
    uint8_t *ptr;

    if ( args->resource_type < XENAML_RESOURCE_TYPE_MIN )
        return NULL;

    rc = xenaml_validate_address_values(args->common_args.is_min_fixed,
                                        args->common_args.is_max_fixed,
                                        &args->address_args);
    if ( !rc )
        return NULL;

    length = xenaml_find_space_desc_length(&args->source_args, 0x0D);

    rnode = xenaml_resource_node(length + XENAML_LARGE_RES_TAG_SIZE, pma);
    if ( rnode == NULL )
        return NULL;
    xenaml_large_res_header(rnode, XENAML_LARGE_RES_WORD_ADDR_SPACE, length);
    rnode->buffer[3] = args->resource_type;
    rnode->buffer[4] = xenaml_general_flags(&args->common_args);
    rnode->buffer[5] = args->type_specific_flags;
    ptr = &rnode->buffer[6];
    xenaml_write_word_address_values(&ptr, &args->address_args);
    xenaml_write_source_values(ptr, &args->source_args);

    return rnode;
}

EXTERNAL void*
xenaml_extended_io(struct xenaml_extended_io_args *args,
                   void *pma)
{
    struct xenaml_node *rnode;
    uint8_t tsf = 0;
    xenaml_bool rc;
    uint16_t length;
    uint8_t *ptr;

    rc = xenaml_io_type_flags_check(args->isa_ranges,
                                    args->translation_type,
                                    args->translation_density,
                                    &tsf);
    if ( !rc )
        return NULL;

    rc = xenaml_validate_address_values(args->common_args.is_min_fixed,
                                        args->common_args.is_max_fixed,
                                        &args->address_args);
    if ( !rc )
        return NULL;

    length = xenaml_find_space_desc_length(NULL, 0x35);

    rnode = xenaml_resource_node(length + XENAML_LARGE_RES_TAG_SIZE, pma);
    if ( rnode == NULL )
        return NULL;
    xenaml_large_res_header(rnode, XENAML_LARGE_RES_EXTENDED_ADDR_SPACE, length);
    rnode->buffer[3] = XENAML_RESOURCE_TYPE_IO_RANGE;
    rnode->buffer[4] = xenaml_general_flags(&args->common_args);
    rnode->buffer[5] = tsf;
    rnode->buffer[6] = 1; /* Revision ID */
    ptr = &rnode->buffer[7];
    xenaml_write_qword_address_values(&ptr, &args->address_args);
    xenaml_write_qword(&ptr, args->type_specific_attributes);

    return rnode;
}

EXTERNAL void*
xenaml_extended_memory(struct xenaml_extended_memory_args *args,
                       void *pma)
{
    struct xenaml_node *rnode;
    uint8_t tsf = 0;
    xenaml_bool rc;
    uint16_t length;
    uint8_t *ptr;

    rc = xenaml_memory_type_flags_check(args->cacheable,
                                        args->read_write,
                                        args->memory_type,
                                        args->translation_type,
                                        &tsf);
    if ( !rc )
        return NULL;

    rc = xenaml_validate_address_values(args->common_args.is_min_fixed,
                                        args->common_args.is_max_fixed,
                                        &args->address_args);
    if ( !rc )
        return NULL;

    length = xenaml_find_space_desc_length(NULL, 0x35);

    rnode = xenaml_resource_node(length + XENAML_LARGE_RES_TAG_SIZE, pma);
    if ( rnode == NULL )
        return NULL;
    xenaml_large_res_header(rnode, XENAML_LARGE_RES_EXTENDED_ADDR_SPACE, length);
    rnode->buffer[3] = XENAML_RESOURCE_TYPE_MEMORY_RANGE;
    rnode->buffer[4] = xenaml_general_flags(&args->common_args);
    rnode->buffer[5] = tsf;
    rnode->buffer[6] = 1; /* Revision ID */
    ptr = &rnode->buffer[7];
    xenaml_write_qword_address_values(&ptr, &args->address_args);
    xenaml_write_qword(&ptr, args->type_specific_attributes);

    return rnode;
}

EXTERNAL void*
xenaml_extended_space(struct xenaml_extended_space_args *args,
                      void *pma)
{
    struct xenaml_node *rnode;
    xenaml_bool rc;
    uint16_t length;
    uint8_t *ptr;

    if ( args->resource_type < XENAML_RESOURCE_TYPE_MIN )
        return NULL;

    rc = xenaml_validate_address_values(args->common_args.is_min_fixed,
                                        args->common_args.is_max_fixed,
                                        &args->address_args);
    if ( !rc )
        return NULL;

    length = xenaml_find_space_desc_length(NULL, 0x35);

    rnode = xenaml_resource_node(length + XENAML_LARGE_RES_TAG_SIZE, pma);
    if ( rnode == NULL )
        return NULL;
    xenaml_large_res_header(rnode, XENAML_LARGE_RES_EXTENDED_ADDR_SPACE, length);
    rnode->buffer[3] = args->resource_type;
    rnode->buffer[4] = xenaml_general_flags(&args->common_args);
    rnode->buffer[5] = args->type_specific_flags;
    rnode->buffer[6] = 1; /* Revision ID */
    ptr = &rnode->buffer[7];
    xenaml_write_qword_address_values(&ptr, &args->address_args);
    xenaml_write_qword(&ptr, args->type_specific_attributes);

    return rnode;
}

EXTERNAL void*
xenaml_interrupt(struct xenaml_interrupt_args *args,
                 void *pma)
{
    struct xenaml_node *rnode;
    uint16_t length;
    uint8_t *ptr;
    uint8_t i;

    if ( (args->interrupts == NULL)||(args->count < 1)||
         (args->resource_usage >= XENAML_RESOURCE_USAGE_MAX)||
         (args->edge_level >= XENAML_IRQ_MODE_MAX)||
         (args->active_level >= XENAML_IRQ_ACTIVE_MAX) )
        return NULL;

    /* Min length accounts for 1 interrupt */
    length = xenaml_find_space_desc_length(&args->source_args, 0x06);
    length += (args->count - 1)*sizeof(uint32_t);

    rnode = xenaml_resource_node(length + XENAML_LARGE_RES_TAG_SIZE, pma);
    if ( rnode == NULL )
        return NULL;
    xenaml_large_res_header(rnode, XENAML_LARGE_RES_INTERRUPT, length);
    rnode->buffer[3] = (args->shared) ? (1 << 3) : 0;
    rnode->buffer[3] |= (0x04 & (args->active_level << 2));
    rnode->buffer[3] |= (0x02 & (args->edge_level << 1));
    rnode->buffer[3] |= (0x01 & args->resource_usage);
    rnode->buffer[4] = args->count;
    ptr = &rnode->buffer[5];
    for ( i = 0; i < args->count; i++ )
        xenaml_write_dword(&ptr, args->interrupts[i]);
    xenaml_write_source_values(ptr, &args->source_args);

    return rnode;
}

EXTERNAL void*
xenaml_register(uint8_t address_space_keyword,
                uint8_t register_bit_width,
                uint8_t register_bit_offset,
                uint64_t register_address,
                enum xenaml_register_access_size access_size,
                void *pma)
{
    struct xenaml_node *rnode;
    uint16_t length;
    uint8_t *ptr;

    if ( (address_space_keyword > XENAML_ADR_SPACE_SMBUS)&&
         (address_space_keyword != XENAML_ADR_SPACE_FIXED_HARDWARE) )
        return NULL;
    if ( access_size > XENAML_REGISTER_ACCESS_MAX )
        return NULL;

    length = xenaml_find_space_desc_length(NULL, 0x0C);

    rnode = xenaml_resource_node(length + XENAML_LARGE_RES_TAG_SIZE, pma);
    if ( rnode == NULL )
        return NULL;
    xenaml_large_res_header(rnode, XENAML_LARGE_RES_REGISTER, length);
    rnode->buffer[3] = address_space_keyword;
    rnode->buffer[4] = register_bit_width;
    rnode->buffer[5] = register_bit_offset;
    rnode->buffer[6] = access_size;
    ptr = &rnode->buffer[7];
    xenaml_write_qword(&ptr, register_address);

    return rnode;
}

EXTERNAL void*
xenaml_resource_template(void *resources,
                         void *pma)
{
    struct xenaml_node *bnode, *lnode;
    uint8_t pkg_len_buf[4];
    uint32_t total = 0, length, pkg_len_size;

    if ( resources == NULL )
        return NULL;

    /* Need to calculate the size of all the resources to setup the buffer
     * size field and the pkg length. Remember that a ResourceTemplate is
     * actually just a Buffer.
     */
    xenaml_calculate_length(resources, &total);

    /* Create a length specifier and chain on the kids */
    lnode = xenaml_integer(total, XENAML_INT_OPTIMIZE, pma);
    if ( lnode == NULL )
        return NULL;
    length = lnode->length + total;
    xenaml_chain_peers(lnode, resources, NULL);

    /* Determine the package length values based off the size of the child node(s) */
    pkg_len_size = xenaml_package_length(&pkg_len_buf[0], length);

    /* Total length is the buffer op + pkg len, rest of the buffer is chained as children */
    bnode = xenaml_alloc_node(pma, (1 + pkg_len_size), 1);
    if ( bnode == NULL )
    {
        if ( pma == NULL )
            xenaml_delete_list(lnode); /* free length and raw data node if present */
        return NULL;
    }
    bnode->op = AML_BUFFER_OP;
    bnode->flags = XENAML_FLAG_NODE_PACKAGE|XENAML_FLAG_RESOURCE_TEMPLATE;
    bnode->buffer[0] = AML_BUFFER_OP;
    memcpy(&bnode->buffer[1], &pkg_len_buf[0], pkg_len_size);

    /* Chain the inner node(s) on as children of the buffer */
    xenaml_chain_children(bnode, lnode, NULL);

    return bnode;
}
