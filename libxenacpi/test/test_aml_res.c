/*
 * test_aml_res.c
 *
 * XEN AML test resource creating code
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

#include "project_test.h"
#define ACPI_MACHINE_WIDTH 32
#include "actypes.h"
#include "xenacpi.h"

static void* make_res_device(const char *eisaid,
                             uint32_t uid,
                             void *peers,
                             const char *devn,
                             void *pma)
{
    void *a, *h, *u;

    /* Name (_HID, EisaId (<eisaid>)) */
    a = xenaml_eisaid(eisaid, pma);
    assert(a != NULL);
    h = xenaml_name_declaration("_HID", a, pma);
    assert(h != NULL);

    /* Name (_UID, <uid>) */
    a = xenaml_integer(uid, XENAML_INT_OPTIMIZE, pma);
    assert(a != NULL);
    u = xenaml_name_declaration("_UID", a, pma);
    assert(u != NULL);

    xenaml_chain_peers(h, u, NULL);
    xenaml_chain_peers(u, peers, NULL);

    return xenaml_device(devn, h, pma);
}

static void* make_prs_lnka(void *pma)
{
    void *a, *b, *e;
    uint8_t irqs1[10] = {1,3,4,5,6,7,10,12,14,15};
    uint8_t irqs2[1] = {2};

    a = xenaml_irq(XENAML_IRQ_MODE_LEVEL,
                   XENAML_IRQ_ACTIVE_HIGH,
                   1, irqs1, 10, pma);
    assert(a != NULL);
    b = xenaml_irq(XENAML_IRQ_MODE_EDGE,
                   XENAML_IRQ_ACTIVE_LOW,
                   0, irqs2, 1, pma);
    assert(b != NULL);
    e = xenaml_end(pma);
    assert(e != NULL);

    xenaml_chain_peers(a, b, NULL);
    xenaml_chain_peers(b, e, NULL);
    b = xenaml_resource_template(a, pma);
    assert(b != NULL);

    a = xenaml_name_declaration("_PRS", b, pma);
    assert(a != NULL);

    return a;
}

static void* make_crs_ecda(void *pma)
{
    void *b, *f, *n, *e;
    uint8_t chans[2] = {2,4};
    uint8_t vb[3] = {0xA7, 0x45, 0x3D};

    f = xenaml_io(XENAML_IO_DECODE_16,
                  0x00F4, 0x00F8, 1, 0x20, pma);
    assert(f != NULL);
    n = f; b = f;

    f = xenaml_io(XENAML_IO_DECODE_16,
                  0x4000, 0x40F0, 0x2, 0x40, pma);
    assert(f != NULL);
    test_chain_peers(&n, &f);

    f = xenaml_dma(XENAML_DMA_TYPE_COMPAT,
                   XENAML_DMA_TRANSER_SIZE_8_16, 0,
                   chans, 2, pma);
    assert(f != NULL);
    test_chain_peers(&n, &f);

    f = xenaml_fixed_io(0x130, 0x4, pma);
    assert(f != NULL);
    test_chain_peers(&n, &f);

    f = xenaml_vendor_short(vb, 3, pma);
    assert(f != NULL);
    test_chain_peers(&n, &f);

    e = xenaml_end(pma);
    assert(e != NULL);

    xenaml_chain_peers(b, n, NULL);
    xenaml_chain_peers(n, e, NULL);
    f = xenaml_resource_template(b, pma);
    assert(f != NULL);

    b = xenaml_name_declaration("_CRS", f, pma);
    assert(b != NULL);

    return b;
}

static void* make_rbfa_ecda(void *pma)
{
    void *b, *f, *n, *e;
    uint8_t irqs[2] = {5,7};

    f = xenaml_start_dependent_fn(XENAML_DEP_PRIORITY_GOOD,
            XENAML_DEP_PRIORITY_GOOD, pma);
    assert(f != NULL);
    n = f; b = f;

    f = xenaml_io(XENAML_IO_DECODE_16,
                  0x0378, 0x0378, 0x1, 0x8, pma);
    assert(f != NULL);
    test_chain_peers(&n, &f);

    f = xenaml_irq_noflags(irqs, 2, pma);
    assert(f != NULL);
    test_chain_peers(&n, &f);

    f = xenaml_end_dependent_fn(pma);
    assert(f != NULL);
    test_chain_peers(&n, &f);

    e = xenaml_end(pma);
    assert(e != NULL);

    xenaml_chain_peers(b, n, NULL);
    xenaml_chain_peers(n, e, NULL);
    f = xenaml_resource_template(b, pma);
    assert(f != NULL);

    b = xenaml_name_declaration("RBFA", f, pma);
    assert(b != NULL);

    return b;
}

static uint8_t* ssdt_small_resources(uint32_t *length_out)
{
    int r, e;
    void *root, *lnka, *prs, *ecda, *crs, *rbfa, *sb;
    uint8_t *buf;
    void *pma;

    pma = xenaml_create_premem(10000);
    if ( pma == NULL )
    {
        printf("ssdt_small_resources: Failed to create premem allocation block\n");
        return NULL;
    }

    r = xenaml_create_ssdt("SRes", "AMLTEST", 0, pma, &root, &e);
    assert(r == 0);

    prs = make_prs_lnka(pma);
    lnka = make_res_device("PNP0C0F", 0, prs, "LNKA", pma);

    crs = make_crs_ecda(pma);
    rbfa = make_rbfa_ecda(pma);
    xenaml_chain_peers(crs, rbfa, NULL);
    ecda = make_res_device("PNP0C09", 1, crs, "ECDA", pma);

    xenaml_chain_peers(lnka, ecda, NULL);

    sb = xenaml_scope("\\_SB_", lnka, NULL);
    xenaml_chain_children(root, sb, NULL);

    xenaml_write_ssdt(root, &buf, length_out, NULL);

    xenaml_free_premem(pma);    

    return buf;
}

static void* make_crs_ecdc(void *pma)
{
    void *b, *f, *n, *e;
    struct xenaml_qword_memory_args qma = {0};
    struct xenaml_dword_memory_args dma = {0};
    struct xenaml_dword_io_args dia = {0};
    struct xenaml_dword_space_args dsa = {0};
    struct xenaml_word_io_args wia = {0};
    struct xenaml_extended_memory_args ema = {0};

    f = xenaml_memory32_fixed(0, 0xFED4C000, 0x012B4000, pma);
    assert(f != NULL);
    n = f; b = f;

    qma.common_args.resource_usage = XENAML_RESOURCE_USAGE_PRODUCER_CONSUMER;
    qma.common_args.decode = XENAML_DECODE_POS_DECODE;
    qma.common_args.is_min_fixed = 1;
    qma.common_args.is_max_fixed = 1;
    qma.cacheable = XENAML_MEMORY_CACHING_CACHEABLE;
    qma.read_write = 1;
    qma.address_args.address_minimum = 0xC1000000000ULL;
    qma.address_args.address_maximum = 0xC1FFFFFFFFFULL;
    qma.address_args.range_length = 0x1000000000ULL;
    qma.memory_type = XENAML_MEMORY_TYPE_MEMORY;
    qma.translation_type = XENAML_TRANSLATION_TYPE_STATIC;
    f = xenaml_qword_memory(&qma, pma);
    assert(f != NULL);
    test_chain_peers(&n, &f);

    dma.common_args.resource_usage = XENAML_RESOURCE_USAGE_PRODUCER_CONSUMER;
    dma.common_args.decode = XENAML_DECODE_POS_DECODE;
    dma.common_args.is_min_fixed = 1;
    dma.common_args.is_max_fixed = 1;
    dma.cacheable = XENAML_MEMORY_CACHING_CACHEABLE;
    dma.read_write = 1;
    dma.address_args.address_minimum = 0xD8000000;
    dma.address_args.address_maximum = 0xDAFFFFFF;
    dma.address_args.range_length = 0x03000000;
    dma.memory_type = XENAML_MEMORY_TYPE_MEMORY;
    dma.translation_type = XENAML_TRANSLATION_TYPE_STATIC;
    f = xenaml_dword_memory(&dma, pma);
    assert(f != NULL);
    test_chain_peers(&n, &f);

    dia.common_args.resource_usage = XENAML_RESOURCE_USAGE_PRODUCER_CONSUMER;
    dia.common_args.decode = XENAML_DECODE_POS_DECODE;
    dia.common_args.is_min_fixed = 1;
    dia.common_args.is_max_fixed = 1;
    dia.isa_ranges = XENAML_ISA_RANGE_ENTIRE;
    dia.address_args.address_minimum = 0x00000D00;
    dia.address_args.address_maximum = 0x0000FFFF;
    dia.address_args.range_length = 0x0000F300;
    dia.translation_type = XENAML_TRANSLATION_TYPE_STATIC;
    f = xenaml_dword_io(&dia, pma);
    assert(f != NULL);
    test_chain_peers(&n, &f);

    dsa.common_args.resource_usage = XENAML_RESOURCE_USAGE_PRODUCER_CONSUMER;
    dsa.common_args.decode = XENAML_DECODE_POS_DECODE;
    dsa.common_args.is_min_fixed = 1;
    dsa.common_args.is_max_fixed = 1;
    dsa.resource_type = 0xCA;
    dsa.type_specific_flags = 0x6B;
    dsa.address_args.address_minimum = 0xF1000000;
    dsa.address_args.address_maximum = 0xF100FFFF;
    dsa.address_args.range_length = 0x00010000;   
    f = xenaml_dword_space(&dsa, pma);
    assert(f != NULL);
    test_chain_peers(&n, &f);

    wia.common_args.resource_usage = XENAML_RESOURCE_USAGE_PRODUCER_CONSUMER;
    wia.common_args.decode = XENAML_DECODE_POS_DECODE;
    wia.common_args.is_min_fixed = 1;
    wia.common_args.is_max_fixed = 1;
    wia.isa_ranges = XENAML_ISA_RANGE_ENTIRE;
    wia.address_args.address_minimum = 0x0000;
    wia.address_args.address_maximum = 0x0CF7;
    wia.address_args.range_length = 0x0CF8;
    wia.translation_type = XENAML_TRANSLATION_TYPE_STATIC;
    f = xenaml_word_io(&wia, pma);
    assert(f != NULL);
    test_chain_peers(&n, &f);

    ema.common_args.resource_usage = XENAML_RESOURCE_USAGE_PRODUCER_CONSUMER;
    ema.common_args.decode = XENAML_DECODE_POS_DECODE;
    ema.common_args.is_min_fixed = 1;
    ema.common_args.is_max_fixed = 1;
    ema.cacheable = XENAML_MEMORY_CACHING_CACHEABLE;
    ema.read_write = 1;
    ema.address_args.address_minimum = 0x00E4400000000000ULL;
    ema.address_args.address_maximum = 0x00E47FFFFFFFFFFFULL;
    ema.address_args.range_length = 0x0000400000000000ULL;
    ema.type_specific_attributes = 0x0000000000000004ULL;
    ema.memory_type = XENAML_MEMORY_TYPE_MEMORY;
    ema.translation_type = XENAML_TRANSLATION_TYPE_STATIC;
    f = xenaml_extended_memory(&ema, pma);
    assert(f != NULL);
    test_chain_peers(&n, &f);

    e = xenaml_end(pma);
    assert(e != NULL);

    xenaml_chain_peers(b, n, NULL);
    xenaml_chain_peers(n, e, NULL);
    f = xenaml_resource_template(b, pma);
    assert(f != NULL);

    b = xenaml_name_declaration("_CRS", f, pma);
    assert(b != NULL);

    return b;
}

static void* make_prs_ecdd(void *pma)
{
    void *b, *f, *n, *e;
    struct xenaml_dword_memory_args dma = {0};   
    struct xenaml_interrupt_args ia = {0};
    uint32_t ints[2] = {0x00000019, 0x00000023};

    dma.common_args.resource_usage = XENAML_RESOURCE_USAGE_CONSUMER;
    dma.common_args.decode = XENAML_DECODE_POS_DECODE;
    dma.common_args.is_min_fixed = 1;
    dma.common_args.is_max_fixed = 1;
    dma.cacheable = XENAML_MEMORY_CACHING_CACHEABLE;
    dma.read_write = 1;
    dma.address_args.address_minimum = 0xD8000000;
    dma.address_args.address_maximum = 0xD8FFFFFF;
    dma.address_args.range_length = 0x01000000;
    dma.memory_type = XENAML_MEMORY_TYPE_MEMORY;
    dma.translation_type = XENAML_TRANSLATION_TYPE_STATIC;
    dma.source_args.present = 1;
    dma.source_args.resource_source_index = 3;
    dma.source_args.resource_source = "_SB.ECDC";
    f = xenaml_dword_memory(&dma, pma);
    assert(f != NULL);
    n = f; b = f;

    memset(&dma, 0, sizeof(struct xenaml_dword_memory_args));
    dma.common_args.resource_usage = XENAML_RESOURCE_USAGE_CONSUMER;
    dma.common_args.decode = XENAML_DECODE_POS_DECODE;
    dma.cacheable = XENAML_MEMORY_CACHING_CACHEABLE;
    dma.read_write = 1;
    dma.address_args.address_granularity = 0x000001F;
    dma.address_args.address_minimum = 0x00C00000;
    dma.address_args.address_maximum = 0x00E80000;
    dma.address_args.range_length = 0x00040000;
    dma.memory_type = XENAML_MEMORY_TYPE_MEMORY;
    dma.translation_type = XENAML_TRANSLATION_TYPE_STATIC;
    f = xenaml_dword_memory(&dma, pma);
    assert(f != NULL);
    test_chain_peers(&n, &f);

    memset(&dma, 0, sizeof(struct xenaml_dword_memory_args));
    dma.common_args.resource_usage = XENAML_RESOURCE_USAGE_CONSUMER;
    dma.common_args.decode = XENAML_DECODE_POS_DECODE;
    dma.cacheable = XENAML_MEMORY_CACHING_PREFETCH;
    dma.read_write = 1;
    dma.address_args.address_granularity = 0x00003FF;
    dma.address_args.address_minimum = 0x07000000;
    dma.address_args.address_maximum = 0x0740FFFF;
    dma.memory_type = XENAML_MEMORY_TYPE_ACPI;
    dma.translation_type = XENAML_TRANSLATION_TYPE_STATIC;
    f = xenaml_dword_memory(&dma, pma);
    assert(f != NULL);
    test_chain_peers(&n, &f);

    ia.resource_usage = XENAML_RESOURCE_USAGE_CONSUMER;
    ia.edge_level = XENAML_IRQ_MODE_EDGE;
    ia.active_level = XENAML_IRQ_ACTIVE_LOW;
    ia.shared = 0;
    ia.interrupts = ints;
    ia.count = 2;
    f = xenaml_interrupt(&ia, pma);
    assert(f != NULL);
    test_chain_peers(&n, &f);

    f = xenaml_register(XENAML_ADR_SPACE_SYSTEM_IO,
        0x10, 0x00, 0x0000000000001000,
        XENAML_REGISTER_ACCESS_UNDEFINED, pma);
    assert(f != NULL);
    test_chain_peers(&n, &f);

    e = xenaml_end(pma);
    assert(e != NULL);

    xenaml_chain_peers(b, n, NULL);
    xenaml_chain_peers(n, e, NULL);   
    f = xenaml_resource_template(b, pma);
    assert(f != NULL);

    b = xenaml_name_declaration("_PRS", f, pma);
    assert(b != NULL);

    return b;
}

static uint8_t* ssdt_large_resources(uint32_t *length_out)
{
    int r, e;
    void *root, *ecdc, *ecdd, *crs, *prs, *sb;
    uint8_t *buf;
    void *pma;

    pma = xenaml_create_premem(20000);
    if ( pma == NULL )
    {
        printf("ssdt_large_resources: Failed to create premem allocation block\n");
        return NULL;
    }

    r = xenaml_create_ssdt("LRes", "AMLTEST", 0, pma, &root, &e);
    assert(r == 0);

    crs = make_crs_ecdc(pma);
    ecdc = make_res_device("PNP0C09", 1, crs, "ECDC", pma);

    prs = make_prs_ecdd(pma);
    ecdd = make_res_device("PNP0C09", 2, prs, "ECDD", pma);

    xenaml_chain_peers(ecdc, ecdd, NULL);

    sb = xenaml_scope("\\_SB_", ecdc, NULL);
    xenaml_chain_children(root, sb, NULL);

    xenaml_write_ssdt(root, &buf, length_out, NULL);

    xenaml_free_premem(pma);    

    return buf;
}

int test_aml_res(int argc, char* argv[])
{
    uint8_t *buf;
    uint32_t length;
    FILE *fs;

    buf = ssdt_small_resources(&length);
    fs = fopen("ssdt_small_res_gen.aml", "wb");
    assert((buf != NULL)&&(fs != NULL));
    fwrite(buf, length, 1, fs);
    fclose(fs);
    xenacpi_free_buffer(buf);
    printf("Wrote: ssdt_small_res_gen.aml\n");

    buf = ssdt_large_resources(&length);
    fs = fopen("ssdt_large_res_gen.aml", "wb");
    assert((buf != NULL)&&(fs != NULL));
    fwrite(buf, length, 1, fs);
    fclose(fs);
    xenacpi_free_buffer(buf);
    printf("Wrote: ssdt_large_res_gen.aml\n");

    return 0;
}

