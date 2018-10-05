/*
 * test_aml_gen.c
 *
 * XEN AML test generation code
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

static const uint8_t abuf_data[] = {
    0x34, 0xF0, 0xB7, 0x5F, 0x63, 0x2C, 0xE9, 0x45,
    0xBE, 0x91, 0x3D, 0x44, 0xE2, 0xC7, 0x07, 0xE4,
    0x41, 0x41, 0x01, 0x02, 0x79, 0x42, 0xF2, 0x95,
    0x7B, 0x4D, 0x34, 0x43, 0x93, 0x87, 0xAC, 0xCD,
    0x80, 0x00, 0x01, 0x08, 0x18, 0x43, 0x81, 0x2B,
    0x9D, 0x84, 0xA1, 0x90, 0xA8, 0x59, 0xB5, 0xD0,
    0xA0, 0x00, 0xE8, 0x4B, 0x07, 0x47, 0x01, 0xC6,
    0x7E, 0xF6, 0x1C, 0x08
};

static const uint8_t mbf1_data[] = {
    0x0E, 0x23, 0xF5, 0x51, 0x77, 0x96, 0xCD, 0x46
};

void test_chain_peers(void **current, void **next)
{
    int ret, err = 0;

    ret = xenaml_chain_peers(*current, *next, &err);
    if ( ret != 0 )
        printf("chaining peers failed - error: %d\n", err);

    assert((ret == 0)&&(err == 0));

    *current = *next;
}

static uint8_t* ssdt_math(uint32_t *length_out)
{
    void *root;
    int r, e = 0;
    void *picc, *picd, *mat1, *mat2;
    void *sb;
    struct xenaml_args al;
    void *a, *f, *n, *b;
    uint8_t *buf;

    r = xenaml_create_ssdt("Math", "AMLTEST", 0, NULL, &root, &e);
    if ( r != 0 )
    {
        printf("ssdt_math: xenaml_create_ssdt failed - error: %d\n", e);
        return NULL;
    }

    a = xenaml_integer(0, XENAML_INT_OPTIMIZE, NULL);
    picc = xenaml_name_declaration("PICC", a, NULL);
    a = xenaml_integer(0, XENAML_INT_OPTIMIZE, NULL);
    picd = xenaml_name_declaration("PICD", a, NULL);

    al.arg[0] = xenaml_integer(0x10, XENAML_INT_BYTE, NULL);
    al.arg[1] = xenaml_variable(XENAML_VARIABLE_TYPE_LOCAL, 0, NULL);
    al.count = 2;
    f = xenaml_misc(XENAML_MISC_FUNC_STORE, &al, NULL);
    n = f; b = f;

    al.arg[0] = xenaml_variable(XENAML_VARIABLE_TYPE_ARG, 0, NULL);
    al.arg[1] = xenaml_variable(XENAML_VARIABLE_TYPE_LOCAL, 0, NULL);
    al.arg[2] = xenaml_variable(XENAML_VARIABLE_TYPE_LOCAL, 1, NULL);
    al.count = 3;
    f = xenaml_math(XENAML_MATH_FUNC_ADD, &al, NULL);
    test_chain_peers(&n, &f);

    al.arg[0] = xenaml_integer(0x4000, XENAML_INT_OPTIMIZE, NULL);
    al.arg[1] = xenaml_variable(XENAML_VARIABLE_TYPE_ARG, 1, NULL);
    al.arg[2] = xenaml_variable(XENAML_VARIABLE_TYPE_LOCAL, 2, NULL);
    al.count = 3;
    f = xenaml_math(XENAML_MATH_FUNC_SUBTRACT, &al, NULL);
    test_chain_peers(&n, &f);   

    al.arg[0] = xenaml_variable(XENAML_VARIABLE_TYPE_LOCAL, 2, NULL);
    al.arg[1] = xenaml_integer(0x10, XENAML_INT_OPTIMIZE, NULL);
    al.arg[2] = xenaml_integer(0, XENAML_INT_ZERO, NULL);
    al.arg[3] = xenaml_integer(0, XENAML_INT_ZERO, NULL);
    al.count = 4;
    al.arg[0] = xenaml_math(XENAML_MATH_FUNC_DIVIDE, &al, NULL);
    al.arg[1] = xenaml_variable(XENAML_VARIABLE_TYPE_LOCAL, 3, NULL);
    al.count = 2;
    f = xenaml_misc(XENAML_MISC_FUNC_STORE, &al, NULL);
    test_chain_peers(&n, &f);    

    al.arg[0] = xenaml_variable(XENAML_VARIABLE_TYPE_LOCAL, 1, NULL);
    al.arg[1] = xenaml_integer(0x3333, XENAML_INT_OPTIMIZE, NULL);
    al.arg[2] = xenaml_integer(0, XENAML_INT_ZERO, NULL);
    al.count = 3;
    al.arg[0] = xenaml_math(XENAML_MATH_FUNC_MODULO, &al, NULL);
    al.arg[1] = xenaml_variable(XENAML_VARIABLE_TYPE_LOCAL, 4, NULL);
    al.count = 2;
    f = xenaml_misc(XENAML_MISC_FUNC_STORE, &al, NULL);
    test_chain_peers(&n, &f);    

    al.arg[0] = xenaml_variable(XENAML_VARIABLE_TYPE_ARG, 0, NULL);
    al.arg[1] = xenaml_variable(XENAML_VARIABLE_TYPE_ARG, 1, NULL);
    al.arg[2] = xenaml_name_reference("PICD", NULL, NULL);
    al.count = 3;
    f = xenaml_math(XENAML_MATH_FUNC_MULTIPLY, &al, NULL);
    test_chain_peers(&n, &f);

    al.arg[0] = xenaml_variable(XENAML_VARIABLE_TYPE_LOCAL, 1, NULL);
    al.count = 1;
    f = xenaml_math(XENAML_MATH_FUNC_INCREMENT, &al, NULL);
    test_chain_peers(&n, &f);    

    al.arg[0] = xenaml_variable(XENAML_VARIABLE_TYPE_LOCAL, 2, NULL);
    al.count = 1;
    f = xenaml_math(XENAML_MATH_FUNC_DECREMENT, &al, NULL);
    test_chain_peers(&n, &f);    

    mat1 = xenaml_method("MAT1", 2, 1, b, NULL);

    al.arg[0] = xenaml_integer(0x4040, XENAML_INT_WORD, NULL);
    al.arg[1] = xenaml_variable(XENAML_VARIABLE_TYPE_LOCAL, 0, NULL);
    al.count = 2;
    f = xenaml_misc(XENAML_MISC_FUNC_STORE, &al, NULL);
    n = f; b = f;

    al.arg[0] = xenaml_integer(0x4044, XENAML_INT_WORD, NULL);
    al.arg[1] = xenaml_variable(XENAML_VARIABLE_TYPE_LOCAL, 1, NULL);
    al.count = 2;
    f = xenaml_misc(XENAML_MISC_FUNC_STORE, &al, NULL);
    test_chain_peers(&n, &f);    

    al.arg[0] = xenaml_variable(XENAML_VARIABLE_TYPE_LOCAL, 0, NULL);
    al.arg[1] = xenaml_variable(XENAML_VARIABLE_TYPE_LOCAL, 1, NULL);
    al.arg[2] = xenaml_variable(XENAML_VARIABLE_TYPE_LOCAL, 2, NULL);
    al.count = 3;
    f = xenaml_math(XENAML_MATH_FUNC_AND, &al, NULL);
    test_chain_peers(&n, &f);
    
    al.arg[0] = xenaml_variable(XENAML_VARIABLE_TYPE_LOCAL, 0, NULL);
    al.arg[1] = xenaml_variable(XENAML_VARIABLE_TYPE_LOCAL, 1, NULL);
    al.arg[2] = xenaml_variable(XENAML_VARIABLE_TYPE_LOCAL, 3, NULL);
    al.count = 3;
    f = xenaml_math(XENAML_MATH_FUNC_OR, &al, NULL);    
    test_chain_peers(&n, &f);

    al.arg[0] = xenaml_integer(0xCCCC, XENAML_INT_WORD, NULL);
    al.arg[1] = xenaml_variable(XENAML_VARIABLE_TYPE_LOCAL, 0, NULL);
    al.arg[2] = xenaml_variable(XENAML_VARIABLE_TYPE_LOCAL, 1, NULL);
    al.count = 3;
    f = xenaml_math(XENAML_MATH_FUNC_XOR, &al, NULL);
    test_chain_peers(&n, &f);

    al.arg[0] = xenaml_variable(XENAML_VARIABLE_TYPE_LOCAL, 3, NULL);
    al.arg[1] = xenaml_integer(0, XENAML_INT_ZERO, NULL);
    al.count = 2;
    al.arg[0] = xenaml_math(XENAML_MATH_FUNC_NOT, &al, NULL);
    al.arg[1] = xenaml_variable(XENAML_VARIABLE_TYPE_LOCAL, 2, NULL);
    al.count = 2;
    f = xenaml_misc(XENAML_MISC_FUNC_STORE, &al, NULL);
    test_chain_peers(&n, &f);

    al.arg[0] = xenaml_variable(XENAML_VARIABLE_TYPE_ARG, 0, NULL);
    al.arg[1] = xenaml_integer(8, XENAML_INT_OPTIMIZE, NULL);
    al.arg[2] = xenaml_name_reference("PICC", NULL, NULL);
    al.count = 3;
    f = xenaml_math(XENAML_MATH_FUNC_SHIFTRIGHT, &al, NULL);
    test_chain_peers(&n, &f);

    al.arg[0] = xenaml_integer(0x88888888, XENAML_INT_OPTIMIZE, NULL);
    al.arg[1] = xenaml_variable(XENAML_VARIABLE_TYPE_LOCAL, 1, NULL);
    al.arg[2] = xenaml_variable(XENAML_VARIABLE_TYPE_LOCAL, 0, NULL);
    al.count = 3;
    f = xenaml_math(XENAML_MATH_FUNC_NAND, &al, NULL);
    test_chain_peers(&n, &f);    

    mat2 = xenaml_method("MAT2", 1, 1, b, NULL);

    /* assemble it all */
    r = xenaml_chain_peers(picc, picd, &e);
    assert((r == 0)&&(e == 0));
    r = xenaml_chain_peers(picd, mat1, &e);
    assert((r == 0)&&(e == 0));
    r = xenaml_chain_peers(mat1, mat2, &e);
    assert((r == 0)&&(e == 0));

    sb = xenaml_scope("\\_SB_", picc, NULL);
    xenaml_chain_children(root, sb, NULL);

    xenaml_write_ssdt(root, &buf, length_out, NULL);

    xenaml_delete_node(root);

    return buf;
}

static uint8_t* ssdt_logic(uint32_t *length_out)
{
    void *root;
    int r, e;
    void *lgcx, *log1, *log2;
    void *sb;
    struct xenaml_args al;
    void *in, *en;
    void *v1, *v2, *v3;
    uint8_t *buf;

    r = xenaml_create_ssdt("Logic", "AMLTEST", 0, NULL, &root, &e);

    v1 = xenaml_integer(0xA5A5A5A5, XENAML_INT_OPTIMIZE, NULL);
    lgcx = xenaml_name_declaration("LGCX", v1, NULL);

    al.arg[0] = xenaml_integer(0x1, XENAML_INT_ONE, NULL);
    al.arg[1] = xenaml_variable(XENAML_VARIABLE_TYPE_LOCAL, 0, NULL);
    al.count = 2;
    v1 = xenaml_misc(XENAML_MISC_FUNC_STORE, &al, NULL);

    al.arg[0] = xenaml_variable(XENAML_VARIABLE_TYPE_ARG, 0, NULL);
    al.arg[1] = xenaml_variable(XENAML_VARIABLE_TYPE_LOCAL, 0, NULL);
    al.count = 2;
    v2 = xenaml_logic(XENAML_LOGIC_FUNC_EQUAL, &al, NULL);

    al.arg[0] = xenaml_variable(XENAML_VARIABLE_TYPE_ARG, 0, NULL);
    al.count = 1;
    al.arg[0] = xenaml_logic(XENAML_LOGIC_FUNC_NOT, &al, NULL);
    al.arg[1] = xenaml_name_reference("LGCX", NULL, NULL);
    al.count = 2;
    v3 = xenaml_misc(XENAML_MISC_FUNC_STORE, &al, NULL);

    in = xenaml_if(v2, v3, NULL);

    al.arg[0] = xenaml_variable(XENAML_VARIABLE_TYPE_LOCAL, 0, NULL);
    al.arg[1] = xenaml_variable(XENAML_VARIABLE_TYPE_ARG, 0, NULL);
    al.count = 2;
    al.arg[0] = xenaml_logic(XENAML_LOGIC_FUNC_LESSTHAN, &al, NULL);
    al.arg[1] = xenaml_name_reference("LGCX", NULL, NULL);
    al.count = 2;
    v3 = xenaml_misc(XENAML_MISC_FUNC_STORE, &al, NULL);

    en = xenaml_else(v3, NULL);

    xenaml_chain_peers(v1, in, NULL);
    xenaml_chain_peers(in, en, NULL);

    log1 = xenaml_method("LOG1", 1, 1, v1, NULL);

    al.arg[0] = xenaml_integer(0x4, XENAML_INT_BYTE, NULL);
    al.arg[1] = xenaml_variable(XENAML_VARIABLE_TYPE_LOCAL, 0, NULL);
    al.count = 2;
    v1 = xenaml_misc(XENAML_MISC_FUNC_STORE, &al, NULL);

    al.arg[0] = xenaml_variable(XENAML_VARIABLE_TYPE_ARG, 0, NULL);
    al.arg[1] = xenaml_variable(XENAML_VARIABLE_TYPE_LOCAL, 0, NULL);
    al.count = 2;
    v2 = xenaml_logic(XENAML_LOGIC_FUNC_GREATEREQUAL, &al, NULL);

    al.arg[0] = xenaml_variable(XENAML_VARIABLE_TYPE_LOCAL, 0, NULL);
    al.arg[1] = xenaml_variable(XENAML_VARIABLE_TYPE_ARG, 1, NULL);
    al.count = 2;
    al.arg[0] = xenaml_logic(XENAML_LOGIC_FUNC_AND, &al, NULL);
    al.arg[1] = xenaml_variable(XENAML_VARIABLE_TYPE_LOCAL, 1, NULL);
    al.count = 2;
    v3 = xenaml_misc(XENAML_MISC_FUNC_STORE, &al, NULL);

    in = xenaml_if(v2, v3, NULL);

    al.arg[0] = xenaml_variable(XENAML_VARIABLE_TYPE_LOCAL, 0, NULL);
    al.arg[1] = xenaml_variable(XENAML_VARIABLE_TYPE_ARG, 1, NULL);
    al.count = 2;
    al.arg[0] = xenaml_logic(XENAML_LOGIC_FUNC_OR, &al, NULL);
    al.arg[1] = xenaml_variable(XENAML_VARIABLE_TYPE_LOCAL, 1, NULL);
    al.count = 2;
    v3 = xenaml_misc(XENAML_MISC_FUNC_STORE, &al, NULL);

    en = xenaml_else(v3, NULL);

    al.arg[0] = xenaml_integer(0xEEEEEEEE, XENAML_INT_OPTIMIZE, NULL);
    al.arg[1] = xenaml_name_reference("LGCX", NULL, NULL);
    al.count = 2;
    v2 = xenaml_misc(XENAML_MISC_FUNC_STORE, &al, NULL);

    al.arg[0] = xenaml_variable(XENAML_VARIABLE_TYPE_LOCAL, 1, NULL);
    al.count = 1;
    v3 = xenaml_misc(XENAML_MISC_FUNC_RETURN, &al, NULL);

    xenaml_chain_peers(v1, in, NULL);
    xenaml_chain_peers(in, en, NULL);
    xenaml_chain_peers(en, v2, NULL);
    xenaml_chain_peers(v2, v3, NULL);

    log2 = xenaml_method("LOG2", 2, 1, v1, NULL);

    /* assemble it all */
    xenaml_chain_peers(lgcx, log1, NULL);
    xenaml_chain_peers(log1, log2, NULL);

    sb = xenaml_scope("\\_SB_", lgcx, NULL);
    xenaml_chain_children(root, sb, NULL);

    xenaml_write_ssdt(root, &buf, length_out, NULL);

    xenaml_delete_node(root);

    return buf;
}

static uint8_t* ssdt_misc(uint32_t *length_out)
{
    void *root;
    int r, e, i;
    void *sb;
    struct xenaml_args al;
    struct xenaml_buffer_init binit;
    void *mbf1, *msc1, *blen, *msc2;
    void *tmp;
    void *c[6];
    uint8_t *buf;

    r = xenaml_create_ssdt("Misc", "AMLTEST", 0, NULL, &root, &e);

    binit.init_type = XENAML_BUFFER_INIT_RAWDATA;
    binit.aml_buffer.aml_rawdata.buffer = mbf1_data;
    binit.aml_buffer.aml_rawdata.raw_length = sizeof(mbf1_data);
    tmp = xenaml_buffer(&binit, NULL);
    mbf1 = xenaml_name_declaration("MBF1", tmp, NULL);

    al.arg[0] = xenaml_integer(0x41414141, XENAML_INT_DWORD, NULL);
    al.arg[1] = xenaml_variable(XENAML_VARIABLE_TYPE_LOCAL, 0, NULL);
    al.count = 2;
    c[0] = xenaml_misc(XENAML_MISC_FUNC_TOBUFFER, &al, NULL);

    al.arg[0] = xenaml_string("TESTSTR", NULL);
    al.arg[1] = xenaml_variable(XENAML_VARIABLE_TYPE_LOCAL, 1, NULL);
    al.count = 2;
    c[1] = xenaml_misc(XENAML_MISC_FUNC_TOBUFFER, &al, NULL);

    al.arg[0] = xenaml_variable(XENAML_VARIABLE_TYPE_ARG, 0, NULL);
    al.arg[1] = xenaml_integer(0, XENAML_INT_ZERO, NULL);
    al.count = 2;
    al.arg[0] = xenaml_misc(XENAML_MISC_FUNC_TOBUFFER, &al, NULL);
    al.arg[1] = xenaml_variable(XENAML_VARIABLE_TYPE_LOCAL, 2, NULL);
    al.count = 2;
    c[2] = xenaml_misc(XENAML_MISC_FUNC_STORE, &al, NULL);

    al.arg[0] = xenaml_integer(1024, XENAML_INT_OPTIMIZE, NULL);
    al.arg[1] = xenaml_variable(XENAML_VARIABLE_TYPE_LOCAL, 3, NULL);
    al.count = 2;
    c[3] = xenaml_misc(XENAML_MISC_FUNC_TODECSTRING, &al, NULL);

    al.arg[0] = xenaml_integer(0x5555, XENAML_INT_OPTIMIZE, NULL);
    al.arg[1] = xenaml_variable(XENAML_VARIABLE_TYPE_LOCAL, 4, NULL);
    al.count = 2;
    c[4] = xenaml_misc(XENAML_MISC_FUNC_TOHEXSTRING, &al, NULL);

    al.arg[0] = xenaml_name_reference("MBF1", NULL, NULL);
    al.arg[1] = xenaml_integer(0x08, XENAML_INT_OPTIMIZE, NULL);
    al.arg[2] = xenaml_variable(XENAML_VARIABLE_TYPE_ARG, 1, NULL);
    al.count = 3;
    c[5] = xenaml_misc(XENAML_MISC_FUNC_TOSTRING, &al, NULL);

    for ( i = 0; i < 5; i++ )
        xenaml_chain_peers(c[i], c[i + 1], NULL);

    msc1 = xenaml_method("MSC1", 2, 1, c[0], NULL);

    c[0] = xenaml_integer(0x333, XENAML_INT_OPTIMIZE, NULL);
    blen = xenaml_name_declaration("BLEN", c[0], NULL);

    al.arg[0] = xenaml_integer(0x110, XENAML_INT_OPTIMIZE, NULL);
    al.arg[1] = xenaml_variable(XENAML_VARIABLE_TYPE_LOCAL, 0, NULL);
    al.count = 2;
    c[0] = xenaml_misc(XENAML_MISC_FUNC_STORE, &al, NULL);

    binit.init_type = XENAML_BUFFER_INIT_VARLEN;
    binit.aml_buffer.aml_varlen.var_type = XENAML_VARIABLE_TYPE_LOCAL;
    binit.aml_buffer.aml_varlen.var_num = 0;
    al.arg[0] = xenaml_buffer(&binit, NULL);
    al.arg[1] = xenaml_variable(XENAML_VARIABLE_TYPE_LOCAL, 1, NULL);
    al.count = 2;
    c[1] = xenaml_misc(XENAML_MISC_FUNC_STORE, &al, NULL);

    binit.init_type = XENAML_BUFFER_INIT_INTLEN;
    binit.aml_buffer.aml_intlen.length = 0x88;
    al.arg[0] = xenaml_buffer(&binit, NULL);
    al.arg[1] = xenaml_variable(XENAML_VARIABLE_TYPE_ARG, 0, NULL);
    al.count = 2;
    c[2] = xenaml_misc(XENAML_MISC_FUNC_STORE, &al, NULL);

    binit.init_type = XENAML_BUFFER_INIT_NAMELEN;
    memcpy(&binit.aml_buffer.aml_namelen.name[0], "BLEN", ACPI_NAME_SIZE);
    al.arg[0] = xenaml_buffer(&binit, NULL);
    al.arg[1] = xenaml_variable(XENAML_VARIABLE_TYPE_ARG, 0, NULL);
    al.count = 2;
    c[3] = xenaml_misc(XENAML_MISC_FUNC_STORE, &al, NULL);

    for ( i = 0; i < 3; i++ )
        xenaml_chain_peers(c[i], c[i + 1], NULL);

    msc2 = xenaml_method("MSC2", 2, 1, c[0], NULL);

    xenaml_chain_peers(mbf1, msc1, NULL);
    xenaml_chain_peers(msc1, blen, NULL);
    xenaml_chain_peers(blen, msc2, NULL);

    sb = xenaml_scope("\\_SB_", mbf1, NULL);
    xenaml_chain_children(root, sb, NULL);

    xenaml_write_ssdt(root, &buf, length_out, NULL);

    xenaml_delete_node(root);

    return buf;
}

static uint8_t* ssdt_sync(uint32_t *length_out)
{
    void *root;
    int r, e = 0;
    void *sb, *m[4];
    struct xenaml_args al;
    void *f, *n, *b;
    uint8_t *buf;
    void *pma;

    pma = xenaml_create_premem(10000);
    if ( pma == NULL )
    {
        printf("ssdt_sync: Failed to create premem allocation block\n");
        return NULL;
    }

    r = xenaml_create_ssdt("Sync", "AMLTEST", 0, pma, &root, &e);
    if ( r != 0 )
    {
        printf("ssdt_sync: xenaml_create_ssdt failed - error: %d\n", e);
        xenaml_free_premem(pma);
        return NULL;
    }    

    f = xenaml_acquire("GPIX", XENAML_SYNC_NO_TIMEOUT, pma);
    n = f; b = f;

    al.arg[0] = xenaml_variable(XENAML_VARIABLE_TYPE_ARG, 0, pma);
    al.arg[1] = xenaml_name_reference("GPID", NULL, pma);
    al.count = 2;
    f = xenaml_misc(XENAML_MISC_FUNC_STORE, &al, pma);
    test_chain_peers(&n, &f);

    f = xenaml_release("GPIX", pma);
    test_chain_peers(&n, &f);

    al.arg[0] = xenaml_integer(0x0A, XENAML_INT_OPTIMIZE, pma);
    al.count = 1;
    f = xenaml_misc(XENAML_MISC_FUNC_SLEEP, &al, pma);
    test_chain_peers(&n, &f);

    m[0] = xenaml_method("GPIZ", 1, 1, b, pma);

    f = xenaml_wait("BEVT", XENAML_SYNC_NO_TIMEOUT, pma);
    al.arg[1] = xenaml_name_reference("BEVD", NULL, pma);
    al.count = 1;
    n = xenaml_misc(XENAML_MISC_FUNC_RETURN, &al, pma);
    xenaml_chain_peers(f, n, NULL);

    m[1] = xenaml_method("BEVW", 1, 1, f, pma);
    xenaml_chain_peers(m[0], m[1], NULL);

    al.arg[0] = xenaml_variable(XENAML_VARIABLE_TYPE_ARG, 0, pma);
    al.arg[1] = xenaml_name_reference("BEVD", NULL, pma);
    al.count = 2;
    f = xenaml_misc(XENAML_MISC_FUNC_STORE, &al, pma);
    n = xenaml_signal("BEVT", pma);
    xenaml_chain_peers(f, n, NULL);

    m[2] = xenaml_method("BEVS", 1, 1, f, pma);
    xenaml_chain_peers(m[1], m[2], NULL);

    f = xenaml_reset("BEVT", pma);
    al.arg[0] = xenaml_integer(0x32, XENAML_INT_OPTIMIZE, pma);
    al.count = 1;
    n = xenaml_misc(XENAML_MISC_FUNC_STALL, &al, pma);
    xenaml_chain_peers(f, n, NULL);

    m[3] = xenaml_method("BEVR", 1, 1, f, pma);
    xenaml_chain_peers(m[2], m[3], NULL);

    f = xenaml_name_declaration("GPID",
                                xenaml_integer(0, XENAML_INT_ZERO, pma),
                                pma);
    n = f; b = f;

    f = xenaml_mutex("GPIX", 2, pma);
    test_chain_peers(&n, &f);

    f = xenaml_name_declaration("BEVD",
                                xenaml_integer(0x33, XENAML_INT_BYTE, pma),
                                pma);
    test_chain_peers(&n, &f);

    f = xenaml_event("BEVT", pma);
    test_chain_peers(&n, &f);

    xenaml_chain_peers(n, m[0], NULL);

    sb = xenaml_scope("\\_SB_", b, NULL);
    xenaml_chain_children(root, sb, NULL);

    xenaml_write_ssdt(root, &buf, length_out, NULL);

    xenaml_free_premem(pma);

    return buf;
}

static void* ssdt_make_op_region(void *pma)
{
    struct xenaml_field_unit ens[5];
    void *op[2] = {0};

    memcpy(&ens[0].aml_field.aml_name.name[0], "P96_", ACPI_NAME_SIZE);
    ens[0].aml_field.aml_name.size_in_bits = 0x8;
    ens[0].type = XENAML_FIELD_TYPE_NAME;

    memcpy(&ens[1].aml_field.aml_name.name[0], "P97_", ACPI_NAME_SIZE);
    ens[1].aml_field.aml_name.size_in_bits = 0x8;
    ens[1].type = XENAML_FIELD_TYPE_NAME;

    memcpy(&ens[2].aml_field.aml_name.name[0], "P98_", ACPI_NAME_SIZE);
    ens[2].aml_field.aml_name.size_in_bits = 0x8;
    ens[2].type = XENAML_FIELD_TYPE_NAME;

    ens[3].aml_field.aml_offset.bits_to_offset = 8;
    ens[3].type = XENAML_FIELD_TYPE_OFFSET;

    memcpy(&ens[4].aml_field.aml_name.name[0], "P100", ACPI_NAME_SIZE);
    ens[4].aml_field.aml_name.size_in_bits = 0x20;
    ens[4].type = XENAML_FIELD_TYPE_NAME;

    op[0] = xenaml_op_region("OPR1",
                             XENAML_ADR_SPACE_SYSTEM_IO,
                             0x96,
                             0x8,
                             pma);
    op[1] = xenaml_field("OPR1",                         
                         XENAML_FIELD_ACCESS_TYPE_BYTE,
                         XENAML_FIELD_LOCK_NEVER,
                         XENAML_FIELD_UPDATE_PRESERVE,
                         &ens[0],
                         5,
                         pma);
    
    xenaml_chain_peers(op[0], op[1], NULL);
    return op[0];
}

static void* ssdt_make_init_fn(void *pma)
{
    void *op[2] = {0};
    struct xenaml_args al;

    al.arg[0] = xenaml_integer(100, XENAML_INT_OPTIMIZE, pma);
    al.arg[1] = xenaml_name_reference("P96_", NULL, pma);
    al.count = 2;
    op[0] = xenaml_misc(XENAML_MISC_FUNC_STORE, &al, pma);

    al.arg[0] = xenaml_variable(XENAML_VARIABLE_TYPE_ARG, 0, pma);
    al.arg[1] = xenaml_name_reference("P98_", NULL, pma);
    al.count = 2;
    op[1] = xenaml_misc(XENAML_MISC_FUNC_STORE, &al, pma);

    xenaml_chain_peers(op[0], op[1], NULL);

    op[1] = xenaml_method("INIT", 1, 1, op[0], pma);

    return op[1];
}

static void* ssdt_make_guid_fn(void *pma)
{
    void *op[16] = {0};
    struct xenaml_args al;

    /* Store (101, P96) */
    al.arg[0] = xenaml_integer(101, XENAML_INT_OPTIMIZE, pma);
    al.arg[1] = xenaml_name_reference("P96_", NULL, pma);
    al.count = 2;
    op[0] = xenaml_misc(XENAML_MISC_FUNC_STORE, &al, pma);

    /* Store (0x0, Local0) */
    al.arg[0] = xenaml_integer(0, XENAML_INT_ZERO, pma);
    al.arg[1] = xenaml_variable(XENAML_VARIABLE_TYPE_LOCAL, 0, pma); 
    al.count = 2;
    op[1] = xenaml_misc(XENAML_MISC_FUNC_STORE, &al, pma);

    /* Store (Arg0, Local1) */
    al.arg[0] = xenaml_variable(XENAML_VARIABLE_TYPE_ARG, 0, pma);
    al.arg[1] = xenaml_variable(XENAML_VARIABLE_TYPE_LOCAL, 1, pma);
    al.count = 2;
    op[2] = xenaml_misc(XENAML_MISC_FUNC_STORE, &al, pma);

    /* LLess (Local0, 0x10) */
    al.arg[0] = xenaml_variable(XENAML_VARIABLE_TYPE_LOCAL, 0, pma);
    al.arg[1] = xenaml_integer(0x10, XENAML_INT_OPTIMIZE, pma);
    al.count = 2;
    op[3] = xenaml_logic(XENAML_LOGIC_FUNC_LESSTHAN, &al, pma);

    /* Add (Local1, Local0, Local2) */
    al.arg[0] = xenaml_variable(XENAML_VARIABLE_TYPE_LOCAL, 1, pma);
    al.arg[1] = xenaml_variable(XENAML_VARIABLE_TYPE_LOCAL, 0, pma);
    al.arg[2] = xenaml_variable(XENAML_VARIABLE_TYPE_LOCAL, 2, pma);
    al.count = 3;
    op[4] = xenaml_math(XENAML_MATH_FUNC_ADD, &al, pma);

    /* Index (_WDG, Local2, Zero) */
    al.arg[0] = xenaml_name_reference("ABUF", NULL, pma);
    al.arg[1] = xenaml_variable(XENAML_VARIABLE_TYPE_LOCAL, 2, pma);
    al.arg[2] = xenaml_integer(0, XENAML_INT_ZERO, pma);
    al.count = 3;
    op[5] = xenaml_misc(XENAML_MISC_FUNC_INDEX, &al, pma);

    /* DerefOf (Index (_WDG, Local2)) */
    al.arg[0] = op[5];
    al.count = 1;
    op[6] = xenaml_misc(XENAML_MISC_FUNC_DEREFOF, &al, pma);

    /* Store (DerefOf (Index (_WDG, Local2)), P98) */
    al.arg[0] = op[6];
    al.arg[1] = xenaml_name_reference("P98_", NULL, pma);
    al.count = 2;
    op[7] = xenaml_misc(XENAML_MISC_FUNC_STORE, &al, pma);

    /* Increment (Local0) */
    al.arg[0] = xenaml_variable(XENAML_VARIABLE_TYPE_LOCAL, 0, pma);
    al.count = 1;
    op[8] = xenaml_math(XENAML_MATH_FUNC_INCREMENT, &al, pma);

    /* LGreater (Local2, 0x400) */
    al.arg[0] = xenaml_variable(XENAML_VARIABLE_TYPE_LOCAL, 2, pma);
    al.arg[1] = xenaml_integer(0x400, XENAML_INT_OPTIMIZE, pma);
    al.count = 2;
    op[9] = xenaml_logic(XENAML_LOGIC_FUNC_GREATERTHAN, &al, pma);

    /* Break */
    al.count = 0;
    op[10] = xenaml_misc(XENAML_MISC_FUNC_BREAK, &al, pma);

    /* If ( LGreater (Local2, 0x400)) ... */
    op[11] = xenaml_if(op[9], op[10], pma);

    xenaml_chain_peers(op[4], op[7], NULL);
    xenaml_chain_peers(op[7], op[8], NULL);
    xenaml_chain_peers(op[8], op[11], NULL);

    /* While ( LLess (Local0, 0x10)) ... */
    op[12] = xenaml_while(op[3], op[4], pma);

    xenaml_chain_peers(op[0], op[1], NULL);
    xenaml_chain_peers(op[1], op[2], NULL);
    xenaml_chain_peers(op[2], op[12], NULL);

    op[13] = xenaml_method("GUID", 1, 1, op[0], pma);

    return op[13];
}

static void* ssdt_make_vpd_fn(void *pma)
{
    void *f, *n, *b;

    /* CreateByteField (VDP3, 0x00, VDP4) */
    f = xenaml_create_field(XENAML_CREATE_FIELD_BYTE,
                            "VDP4",
                            "VDP3",
                            0x00,
                            0,
                            pma);
    n = f; b = f;

    /* CreateWordField (VDP3, 0x01, VDP5) */
    f = xenaml_create_field(XENAML_CREATE_FIELD_WORD,
                            "VDP5",
                            "VDP3",
                            0x01,
                            0,
                            pma);
    test_chain_peers(&n, &f);

    /* CreateDWordField (VDP3, 0x03, VDP6) */
    f = xenaml_create_field(XENAML_CREATE_FIELD_DWORD,
                            "VDP6",
                            "VDP3",
                            0x03,
                            0,
                            pma);

    test_chain_peers(&n, &f);

    /* CreateQWordField (VDP3, 0x08, VDP7 */
    f = xenaml_create_field(XENAML_CREATE_FIELD_QWORD,
                            "VDP7",
                            "VDP3",
                            0x08,
                            0,
                            pma);
    test_chain_peers(&n, &f);

    /* Method (VDP2, 2, NotSerialized) */
    return xenaml_method("VDP2", 2, 1, b, pma);
}

static void* ssdt_make_power_resource(void *pma)
{
    void *x, *sta, *on, *off;
    struct xenaml_args al;

    al.arg[0] = xenaml_name_reference("PLVL", NULL, pma);
    al.count = 1;
    x = xenaml_misc(XENAML_MISC_FUNC_RETURN, &al, pma);
    sta = xenaml_method("_STA", 0, 0, x, NULL);

    al.arg[0] = xenaml_integer(0x1, XENAML_INT_ONE, pma);
    al.arg[1] = xenaml_name_reference("PLVL", NULL, pma);
    al.count = 2;
    x = xenaml_misc(XENAML_MISC_FUNC_STORE, &al, pma);
    on = xenaml_method("_ON_", 0, 0, x, NULL);

    al.arg[0] = xenaml_integer(0, XENAML_INT_ZERO, pma);
    al.arg[1] = xenaml_name_reference("PLVL", NULL, pma);
    al.count = 2;
    x = xenaml_misc(XENAML_MISC_FUNC_STORE, &al, pma);
    off = xenaml_method("_OFF", 0, 0, x, pma);

    xenaml_chain_peers(sta, on, NULL);
    xenaml_chain_peers(on, off, NULL);

    return xenaml_power_resource("LPP_", 0x3, 0x0102, sta, pma);
}

static void* ssdt_make_device(void *pma)
{
    struct xenaml_buffer_init binit;
    void *op[20] = {0};
    void *dev;
    
    /* Name (_HID, EisaId ("PNP0A06")) */
    op[0] = xenaml_eisaid("PNP0A06", pma);
    op[1] = xenaml_name_declaration("_HID", op[0], pma);

    /* Name (_UID, 0x00) */
    op[2] = xenaml_integer(0, XENAML_INT_ZERO, pma);
    op[3] = xenaml_name_declaration("_UID", op[2], pma);

    /* Name (ABUF, Buffer (0xC8) ... */
    binit.init_type = XENAML_BUFFER_INIT_RAWDATA;
    binit.aml_buffer.aml_rawdata.buffer = abuf_data;
    binit.aml_buffer.aml_rawdata.raw_length = sizeof(abuf_data);
    op[4] = xenaml_buffer(&binit, pma);
    op[5] = xenaml_name_declaration("ABUF", op[4], pma);

    /* Method (INIT, 1, Serialized) */
    op[6] = ssdt_make_init_fn(pma);

    /* Method (GUID, 1, Serialized) */
    op[7] = ssdt_make_guid_fn(pma);

    /* Name (VDP3, Buffer (0x10) {}) */
    binit.init_type = XENAML_BUFFER_INIT_INTLEN;
    binit.aml_buffer.aml_intlen.length = 0x10;    
    op[8] = xenaml_buffer(&binit, pma);
    op[9] = xenaml_name_declaration("VDP3", op[8], pma);

    /* Method (VDP2, 2, NotSerialized) */
    op[10] = ssdt_make_vpd_fn(pma);

    /* Name (PLVL, 0xFF) */
    op[11] = xenaml_integer(0xFF, XENAML_INT_ONES, pma);
    op[12] = xenaml_name_declaration("PLVL", op[11], pma);

    /* PowerResource (LPP, 0x03, 0x0102) {...} */
    op[13] = ssdt_make_power_resource(pma);    

    xenaml_chain_peers(op[1], op[3], NULL);
    xenaml_chain_peers(op[3], op[5], NULL);
    xenaml_chain_peers(op[5], op[6], NULL);
    xenaml_chain_peers(op[6], op[7], NULL);
    xenaml_chain_peers(op[7], op[9], NULL);
    xenaml_chain_peers(op[9], op[10], NULL);
    xenaml_chain_peers(op[10], op[12], NULL);
    xenaml_chain_peers(op[12], op[13], NULL);

    dev = xenaml_device("DEV0", op[1], pma);
    
    return dev;
}

static void* ssdt_make_thermal_zone(void *pma)
{
    void *x, *tmp, *crt, *tz;
    struct xenaml_args al;

    al.arg[0] = xenaml_integer(0x0139, XENAML_INT_WORD, pma);
    al.count = 1;
    x = xenaml_misc(XENAML_MISC_FUNC_RETURN, &al, pma);
    tmp = xenaml_method("_TMP", 0, 0, x, NULL);

    al.arg[0] = xenaml_integer(0x0FAC, XENAML_INT_WORD, pma);
    al.count = 1;
    x = xenaml_misc(XENAML_MISC_FUNC_RETURN, &al, pma);
    crt = xenaml_method("_CRT", 0, 0, x, pma);

    xenaml_chain_peers(tmp, crt, NULL);

    tz = xenaml_thermal_zone("DM1Z", tmp, pma);
    assert(tz != NULL);

    return xenaml_scope("\\_TZ_", tz, pma);
}

static void* ssdt_make_processors(void *pma)
{
    void *cpu0, *cpu1;

    cpu0 = xenaml_processor("CPU0", 0x00, 0x00001010, 0x06, NULL, pma);
    cpu1 = xenaml_processor("CPU1", 0x01, 0x00001010, 0x06, NULL, pma);
    assert((cpu0 != NULL)&&(cpu1 != NULL));

    xenaml_chain_peers(cpu0, cpu1, NULL);   

    return xenaml_scope("\\_PR_", cpu0, pma);
}

static uint8_t* ssdt_device(uint32_t *length_out)
{
    int r, e;
    void *op, *dev, *sb, *tz, *pr, *root;
    uint8_t *buf;
    void *pma;

    pma = xenaml_create_premem(10000);
    if ( pma == NULL )
    {
        printf("ssdt_device: Failed to create premem allocation block\n");
        return NULL;
    }

    r = xenaml_create_ssdt("Dev", "AMLTEST", 0, pma, &root, &e);

    op = ssdt_make_op_region(pma);
    dev = ssdt_make_device(pma);

    /* chain dev to field node that follows the op region */
    xenaml_chain_peers(xenaml_next(op), dev, NULL);

    sb = xenaml_scope("\\_SB_", op, pma);

    tz = ssdt_make_thermal_zone(pma);

    pr = ssdt_make_processors(pma);

    xenaml_chain_peers(sb, tz, NULL);
    xenaml_chain_peers(tz, pr, NULL);
    xenaml_chain_children(root, sb, NULL);

    xenaml_write_ssdt(root, &buf, length_out, NULL);

    xenaml_free_premem(pma);

    return buf;
}

int test_aml_gen(int argc, char* argv[])
{
    uint8_t *buf;
    uint32_t length;
    FILE *fs;

    xenaml_name_reference("\\_SB_NDD1NDD2NDD3", NULL, NULL);

    buf = ssdt_math(&length);
    fs = fopen("ssdt_math_gen.aml", "wb");
    assert((buf != NULL)&&(fs != NULL));
    fwrite(buf, length, 1, fs);
    fclose(fs);
    xenacpi_free_buffer(buf);
    printf("Wrote: ssdt_math_gen.aml\n");

    buf = ssdt_logic(&length);
    fs = fopen("ssdt_logic_gen.aml", "wb");
    assert((buf != NULL)&&(fs != NULL));
    fwrite(buf, length, 1, fs);
    fclose(fs);
    xenacpi_free_buffer(buf);
    printf("Wrote: ssdt_logic_gen.aml\n");

    buf = ssdt_misc(&length);
    fs = fopen("ssdt_misc_gen.aml", "wb");
    assert((buf != NULL)&&(fs != NULL));
    fwrite(buf, length, 1, fs);
    fclose(fs);
    xenacpi_free_buffer(buf);
    printf("Wrote: ssdt_misc_gen.aml\n");

    buf = ssdt_sync(&length);
    fs = fopen("ssdt_sync_gen.aml", "wb");
    assert((buf != NULL)&&(fs != NULL));
    fwrite(buf, length, 1, fs);
    fclose(fs);
    xenacpi_free_buffer(buf);
    printf("Wrote: ssdt_sync_gen.aml\n");

    buf = ssdt_device(&length);
    fs = fopen("ssdt_device_gen.aml", "wb");
    assert((buf != NULL)&&(fs != NULL));
    fwrite(buf, length, 1, fs);
    fclose(fs);
    xenacpi_free_buffer(buf);
    printf("Wrote: ssdt_device_gen.aml\n");

    return 0;
}

