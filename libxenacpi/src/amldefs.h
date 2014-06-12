/*
 * amldefs.h
 *
 * XEN ACPI AML internal definitions and data.
 *
 * Copyright (c) 2011 Ross Philipson <ross.philipson@citrix.com>
 * Copyright (c) 2011 Citrix Systems, Inc.
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

#ifndef __AMLDEFS_H__
#define __AMLDEFS_H__

#define XENAML_REVISION                2
#define XENAML_ASL_COMPILER_ID         "INTL"
#define XENAML_ASL_COMPILER_REV        0x20061109

#define XENAML_FLAG_NONE               0x00000000
#define XENAML_FLAG_DEFINITION_BLOCK   0x00000001
#define XENAML_FLAG_NODE_PACKAGE       0x00000010
#define XENAML_FLAG_RAW_DATA           0x00000020
#define XENAML_FLAG_NAME_SIMPLE        0x00000100
#define XENAML_FLAG_NAME_RELATIVE      0x00000200
#define XENAML_FLAG_NAME_ROOT          0x00000400
#define XENAML_FLAG_DUAL_OP            0x00001000
#define XENAML_FLAG_NAME_ARGS_ONLY     0x00002000
#define XENAML_FLAG_PREMEM_ALLOC       0x00010000
#define XENAML_FLAG_RESOURCE_TEMPLATE  0x00100000
#define XENAML_FLAG_RESOURCE           0x00200000

#define XENAML_ADR_SPACE_INVALID       (ACPI_ADR_SPACE_TYPE) 0xFF
#define XENAML_TABLE_CS_OFFSET         9
#define XENAML_FIELD_OFFSET_IND        0x00
#define XENAML_MAX_VARIABLE_NUM        0x07
#define XENAML_MAX_ARGUMENT_NUM        0x06
#define XENAML_MAX_PACKAGE_ELEMS       256
#define XENAML_EISAID_STR_LEN          7
#define XENAML_UNASSIGNED_OPCODE       (uint16_t) 0xFFFF
#define XENAML_EXTRA_NAME_BYTES        3
#define XENAML_MAX_SYNC_LEVEL          ((AML_METHOD_SYNC_LEVEL >> 4) & 0xF)

#define XENAML_PACKAGE_LEN_LIMIT1      0x40       /* Encodes 0x3F in 6 bits and 1 byte */
#define XENAML_PACKAGE_LEN_LIMIT2      0x1000     /* Encodes 0xFFF in 4 bits and 1 byte */
#define XENAML_PACKAGE_LEN_LIMIT3      0x100000   /* Encodes 0xFFFFF in 4 bits and 2 bytes */
#define XENAML_PACKAGE_LEN_LIMIT4      0x10000000 /* Encodes 0xFFFFFFF in 4 bits and 3 bytes */

#define XENAML_ALIGN_BYTE 8
#define XENAML_ALIGN_MASK 7

#define XENAML_BYTE_ALIGN(x) (((uintptr_t)x + (XENAML_ALIGN_BYTE - 1)) & ~(XENAML_ALIGN_BYTE - 1))
#define XENAML_MASK_ALIGN(x) (((uintptr_t)x + XENAML_ALIGN_MASK) & ~XENAML_ALIGN_MASK)
#define XENAML_ROUNDUP(x, p) (((x) + (p - 1) ) & ~(p - 1))

#define XENAML_NAME_OP (AML_MULTI_NAME_PREFIX_OP|AML_DUAL_NAME_PREFIX|AML_NAME_OP)

struct xenaml_node {
    struct xenaml_node *next;
    struct xenaml_node *prev;

    uint16_t op;
    uint32_t length;
    uint8_t *buffer;

    uint32_t flags;

    struct xenaml_node *children;
};

struct xenaml_premem {
    uint8_t *next;
    uint32_t size;
    uint32_t free;
};

static const uint32_t int_size_list[] = {
    1, /* XENAML_INT_ZERO */
    1, /* XENAML_INT_ONE */
    1, /* XENAML_INT_ONES */
    1, /* XENAML_INT_BYTE */
    2, /* XENAML_INT_WORD */
    4, /* XENAML_INT_DWORD */
    8, /* XENAML_INT_QWORD */
};

static const uint16_t local_op_list[] = {
    AML_LOCAL0,
    AML_LOCAL1,
    AML_LOCAL2,
    AML_LOCAL3,
    AML_LOCAL4,
    AML_LOCAL5,
    AML_LOCAL6,
    AML_LOCAL7
};

static const uint16_t arg_op_list[] = {
    AML_ARG0,
    AML_ARG1,
    AML_ARG2,
    AML_ARG3,
    AML_ARG4,
    AML_ARG5,
    AML_ARG6
};

struct op_args_info {
    uint16_t op;
    uint16_t count;
    uint32_t flags;
};

static const struct op_args_info math_op_list[] = {
    {AML_ADD_OP, 3, XENAML_FLAG_NONE},
    {AML_SUBTRACT_OP, 3, XENAML_FLAG_NONE},
    {AML_MULTIPLY_OP, 3, XENAML_FLAG_NONE},
    {AML_DIVIDE_OP, 4, XENAML_FLAG_NONE},
    {AML_MOD_OP, 3, XENAML_FLAG_NONE},
    {AML_INCREMENT_OP, 1, XENAML_FLAG_NONE},
    {AML_DECREMENT_OP, 1, XENAML_FLAG_NONE},
    {AML_BIT_AND_OP, 3, XENAML_FLAG_NONE},
    {AML_BIT_NAND_OP, 3, XENAML_FLAG_NONE},
    {AML_BIT_OR_OP, 3, XENAML_FLAG_NONE},
    {AML_BIT_NOR_OP, 3, XENAML_FLAG_NONE},
    {AML_BIT_XOR_OP, 3, XENAML_FLAG_NONE},
    {AML_BIT_NOT_OP, 2, XENAML_FLAG_NONE},
    {AML_SHIFT_LEFT_OP, 3, XENAML_FLAG_NONE},
    {AML_SHIFT_RIGHT_OP, 3, XENAML_FLAG_NONE}
};

static const struct op_args_info logic_op_list[] = {
    {AML_LAND_OP, 2, XENAML_FLAG_NONE},
    {AML_LOR_OP, 2, XENAML_FLAG_NONE},
    {AML_LNOT_OP, 1, XENAML_FLAG_NONE},
    {AML_LEQUAL_OP, 2, XENAML_FLAG_NONE},
    {AML_LGREATER_OP, 2, XENAML_FLAG_NONE},
    {AML_LLESS_OP, 2, XENAML_FLAG_NONE},
    {AML_LNOTEQUAL_OP, 2, XENAML_FLAG_DUAL_OP},
    {AML_LLESSEQUAL_OP, 2, XENAML_FLAG_DUAL_OP},
    {AML_LGREATEREQUAL_OP, 2, XENAML_FLAG_DUAL_OP}
};

static const struct op_args_info misc_op_list[] = {
    {AML_ALIAS_OP, 2, XENAML_FLAG_NAME_ARGS_ONLY},
    {AML_STORE_OP, 2, XENAML_FLAG_NONE},
    {AML_DEREF_OF_OP, 1, XENAML_FLAG_NONE},
    {AML_NOTIFY_OP, 2, XENAML_FLAG_NONE},
    {AML_SIZE_OF_OP, 1, XENAML_FLAG_NONE},
    {AML_INDEX_OP, 3, XENAML_FLAG_NONE},
    {AML_TO_BUFFER_OP, 2, XENAML_FLAG_NONE},
    {AML_TO_DECSTRING_OP, 2, XENAML_FLAG_NONE},
    {AML_TO_HEXSTRING_OP, 2, XENAML_FLAG_NONE},
    {AML_TO_INTEGER_OP, 2, XENAML_FLAG_NONE},
    {AML_TO_STRING_OP, 3, XENAML_FLAG_NONE},
    {AML_CONTINUE_OP, 0, XENAML_FLAG_NONE},
    {AML_RETURN_OP, 1, XENAML_FLAG_NONE},
    {AML_BREAK_OP, 0, XENAML_FLAG_NONE},
    {AML_SLEEP_OP, 1, XENAML_FLAG_DUAL_OP},
    {AML_STALL_OP, 1, XENAML_FLAG_DUAL_OP}
};

static const struct op_args_info create_field_op_list[] = {
    {AML_CREATE_BIT_FIELD_OP, 3, XENAML_FLAG_NONE},
    {AML_CREATE_BYTE_FIELD_OP, 3, XENAML_FLAG_NONE},
    {AML_CREATE_WORD_FIELD_OP, 3, XENAML_FLAG_NONE},
    {AML_CREATE_DWORD_FIELD_OP, 3, XENAML_FLAG_NONE},
    {AML_CREATE_QWORD_FIELD_OP, 3, XENAML_FLAG_NONE},
    {AML_CREATE_FIELD_OP, 4, XENAML_FLAG_DUAL_OP}
};

static INLINE void xenaml_reset_node(struct xenaml_node *node)
{
    node->prev = NULL;
    node->next = NULL;
}

void* xenaml_prealloc(struct xenaml_premem *premem,
                      uint32_t length);
void* xenaml_alloc_node(struct xenaml_premem *premem,
                        uint32_t length,
                        xenaml_bool zero_buf);
void xenaml_write_node(struct xenaml_node *node,
                       uint8_t **buffer_out);
void xenaml_calculate_length(struct xenaml_node *node,
                             uint32_t *length_out);
void xenaml_write_byte(uint8_t **buffer, uint8_t value);
void xenaml_write_word(uint8_t **buffer, uint16_t value);
void xenaml_write_dword(uint8_t **buffer, uint32_t value);
void xenaml_write_qword(uint8_t **buffer, uint64_t value);
uint32_t xenaml_package_length(uint8_t *pkg_len_buf, uint32_t pkg_len);

#endif /* __AMLDEFS_H__ */

