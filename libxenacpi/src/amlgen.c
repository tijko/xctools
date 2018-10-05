/*
 * amlgen.c
 *
 * XEN ACPI AML generator code.
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

static uint8_t
xenaml_hex_char_to_hex(char hex)
{
    if ( hex <= 0x39 )
        return (uint8_t)(hex - 0x30);

    if ( hex <= 0x46 )
        return (uint8_t)(hex - 0x37);

    return (uint8_t)(hex - 0x57);
}

static xenaml_bool
xenaml_check_args(uint16_t count, struct xenaml_args *arg_list)
{
    uint16_t i;

    if ( arg_list->count != count )
        return 0;

    for ( i = 0; i < count; i++ )
    {
        if ( arg_list->arg[i] == NULL )
            return 0;
    }

    return 1;
}

static void
xenaml_chain_args(struct xenaml_node *node, struct xenaml_args *arg_list)
{
    uint16_t i;

    for ( i = 0; i < arg_list->count; i++ )
    {
        if ( i != 0 )
            xenaml_chain_peers(arg_list->arg[i - 1], arg_list->arg[i], NULL);
        else
            xenaml_chain_children(node, arg_list->arg[i], NULL);
    }
}

static uint8_t
xenaml_op_region_values(uint8_t region_space,
                        uint64_t region_offset,
                        uint64_t region_length,
                        void *pma,
                        struct xenaml_node **anode)
{
    uint8_t ret = XENAML_ADR_SPACE_INVALID;

    switch (region_space) {
    case XENAML_ADR_SPACE_SYSTEM_MEMORY:
        if ( region_offset > 0xFFFFFFF || region_length > 0xFFFFFFF )
            *anode = xenaml_integer(region_offset, XENAML_INT_QWORD, pma);
        else
            *anode = xenaml_integer(region_offset, XENAML_INT_DWORD, pma);

        ret = ACPI_ADR_SPACE_SYSTEM_MEMORY;
        break;
    case XENAML_ADR_SPACE_SYSTEM_IO:
        if ( region_offset > 0xFFFF || region_length > 0xFFFF )
            break;
        *anode = xenaml_integer(region_offset, XENAML_INT_WORD, pma);
        ret = ACPI_ADR_SPACE_SYSTEM_IO;
        break;
    case XENAML_ADR_SPACE_PCI_CONFIG:
        if ( region_offset > 0xFFF || region_length > 0xFFF )
            break;
        *anode = xenaml_integer(region_offset, XENAML_INT_WORD, pma);
        ret = ACPI_ADR_SPACE_PCI_CONFIG;
        break;
    default:
        *anode = NULL;
    };

    if ( *anode == NULL )
        ret = XENAML_ADR_SPACE_INVALID;

    return ret;
}

static enum xenaml_int
xenaml_check_integer_type(uint64_t value, enum xenaml_int int_type)
{
    if ( int_type >= XENAML_INT_MAX )
        return XENAML_INT_MAX; /* invalid */

    if ( int_type == XENAML_INT_OPTIMIZE )
    {
        if ( value > 0xFFFFFFFF )
            return XENAML_INT_QWORD;
        else if ( value > 0xFFFF )
            return XENAML_INT_DWORD;
        else if ( value > 0xFF )
            return XENAML_INT_WORD;
        else if ( value == 0xFF )
            return XENAML_INT_ONES;
        else if ( value > 1 )
            return XENAML_INT_BYTE;
        else if ( value == 1 )
            return XENAML_INT_ONE;
        else
            return XENAML_INT_ZERO;
    }

    /* caller requested specific size */
    return int_type;
}

static uint32_t
xenaml_simple_name(const char *name,
                   uint8_t *buffer,
                   uint32_t *flags)
{
    if ( buffer != NULL )
        memcpy(&buffer[0], &name[0], ACPI_NAME_SIZE);
    if ( flags != NULL )
        *flags |= XENAML_FLAG_NAME_SIMPLE;

    return ACPI_NAME_SIZE;
}

static uint32_t
xenaml_relative_name(const char *name,
                     uint32_t length,
                     uint8_t *buffer,
                     uint32_t *flags)
{
    uint32_t i;

    /* Scan past ^ chars - remainder should be a simple NNNN name */
    for (i = 0; i < length; i++)
    {
        if ( name[i] == AML_PARENT_PREFIX )
            continue;
        if ( name[i] == AML_ROOT_PREFIX )
            return 0;
        break;
    }
    if ( strlen(&name[i]) != ACPI_NAME_SIZE )
        return 0;

    /* Copy relative prefixes and name into node with no op */
    if ( buffer != NULL )
        memcpy(&buffer[0], &name[0], length);
    if ( flags != NULL )
        *flags |= XENAML_FLAG_NAME_RELATIVE;

    return length;
}

static uint32_t
xenaml_build_name(const char *name,
                  uint8_t *buffer,
                  uint32_t *flags)
{
    uint32_t nlength, count, total;

    /* Name lengths must be multiples of 4. Double or multi name strings are
     * prefixed with the root char \ so are therefore (4*n + 1) in length.
     * These names are fully quailified paths and have the . path separator
     * taken out. There are also relative paths prefixed by 1 - N ^ chars.
     * The simplest name references and field tags are simply 4 byte names.
     */

    /* When buffer and flags are NULL a name scan is being reguested to
     * validate and find the name's size.
     */

    if ( name == NULL )
        return 0;

    nlength = strlen(name);
    if ( nlength < ACPI_NAME_SIZE )
        return 0;

    /* Handle simple names first */
    if ( nlength == ACPI_NAME_SIZE )
        return xenaml_simple_name(name, buffer, flags);

    /* Handle relative names next */
    if ( name[0] == AML_PARENT_PREFIX )
        return xenaml_relative_name(name, nlength, buffer, flags);

    /* Know that length > ACPI_NAME_SIZE at this point, validate root path */
    if ( name[0] != AML_ROOT_PREFIX )
        return 0;

    name++;
    nlength--;
    count = nlength/4;

    if ( count == 0 || (nlength % 4) > 0 )
        return 0;

    if ( count > 2 )
    {
        if ( buffer != NULL )
        {
            buffer[0] = AML_ROOT_PREFIX;
            buffer[1] = AML_MULTI_NAME_PREFIX_OP;
            buffer[2] = count;
            memcpy(&buffer[3], &name[0], nlength);
        }
        total = nlength + 3;
    }
    else if ( count == 2 )
    {
        if ( buffer != NULL )
        {
            buffer[0] = AML_ROOT_PREFIX;
            buffer[1] = AML_DUAL_NAME_PREFIX;
            memcpy(&buffer[2], &name[0], 2*ACPI_NAME_SIZE);
        }
        total = 2*ACPI_NAME_SIZE + 2;
    }
    else
    {
        if ( buffer != NULL )
        {
            buffer[0] = AML_ROOT_PREFIX;
            memcpy(&buffer[1], &name[0], ACPI_NAME_SIZE);
        }
        total = ACPI_NAME_SIZE + 1;
    }

    if ( flags != NULL )
        *flags |= XENAML_FLAG_NAME_ROOT;

    return total;
}

static uint32_t
xenaml_scan_name(const char *name)
{
    /* Named entities call this first to validate the input name
     * and get the required size. This routine and the format name
     * routine use the same inner functions to keep the name logic
     * in one place.
     */
    return xenaml_build_name(name, NULL, NULL);
}

static void
xenaml_format_name(const char *name,
                   uint8_t *buffer,
                   uint32_t *flags)
{
    uint32_t nlength;

    /* After a successful scan of the name, the caller can
     * then call this to format the name. This operation should
     * not fail unless there is a bug.
     */
    assert(name != NULL);
    assert(buffer != NULL);
    assert(flags != NULL);
    nlength = xenaml_build_name(name, buffer, flags);
    assert(nlength > 0);
}

static int8_t
xenaml_check_package_elements(struct xenaml_node *element,
                              uint32_t *count_out)
{
    /* The elements input should be a linked list of peer elements. Only
     * certain types are permissable. The logic to determine this may not be
     * spot on but we shall see.
     */
    while (element != NULL)
    {
        (*count_out)++;

        switch (element->op)
        {
        case AML_ZERO_OP:
        case AML_ONE_OP:
        case AML_NAME_OP:
        case AML_BYTE_OP:
        case AML_WORD_OP:
        case AML_DWORD_OP:
        case AML_STRING_OP:
        case AML_QWORD_OP:
        case AML_ONES_OP:
            /* These cannot have children */
            if ( element->children != NULL )
                return 0;
            break;
        case AML_ROOT_PREFIX:
            if ( element->length < 2 )
                return 0; /* borked name */
            if ( element->buffer[1] != AML_DUAL_NAME_PREFIX &&
                 element->buffer[1] != AML_MULTI_NAME_PREFIX_OP )
                return 0; /* dorked name */
            /* These cannot have children */
            if ( element->children != NULL )
                return 0;
            break;
        case AML_BUFFER_OP:
        case AML_PACKAGE_OP:
        case AML_VAR_PACKAGE_OP:
            /* These can have children */
            break;
        default:
            return 0;
        };

        element = element->next;
    }

    return 1;
}

EXTERNAL void*
xenaml_nullchar(void *pma)
{
    struct xenaml_node *nnode;

    nnode = xenaml_alloc_node(pma, 1, 1);
    if ( nnode == NULL )
        return NULL;

    nnode->op = AML_NULL_CHAR;

    return nnode;
}

EXTERNAL void*
xenaml_raw_data(const uint8_t *buffer, uint32_t raw_length, void *pma)
{
    struct xenaml_node *nnode;

    if ( buffer == NULL || raw_length == 0 )
        return NULL;

    nnode = xenaml_alloc_node(pma, raw_length, 0);
    if ( nnode == NULL )
        return NULL;

    nnode->op = XENAML_UNASSIGNED_OPCODE;
    nnode->flags = XENAML_FLAG_RAW_DATA;
    memcpy(&nnode->buffer[0], &buffer[0], raw_length);

    return nnode;
}

EXTERNAL void*
xenaml_integer(uint64_t value, enum xenaml_int int_type, void *pma)
{
    struct xenaml_node *nnode;
    uint8_t *ptr;

    int_type = xenaml_check_integer_type(value, int_type);
    if ( int_type == XENAML_INT_MAX )
        return NULL;

    nnode = xenaml_alloc_node(pma, (int_size_list[int_type] + 1), 1);
    if ( nnode == NULL )
        return NULL;
    ptr = nnode->buffer + 1;

    if ( int_type == XENAML_INT_QWORD )
    {
        nnode->op = AML_QWORD_OP;
        nnode->buffer[0] = AML_QWORD_OP;
        xenaml_write_qword(&ptr, value);
        nnode->length = int_size_list[int_type] + 1;
    }
    else if ( int_type == XENAML_INT_DWORD )
    {
        nnode->op = AML_DWORD_OP;
        nnode->buffer[0] = AML_DWORD_OP;
        xenaml_write_dword(&ptr, (uint32_t)(value & 0xFFFFFFFF));
        nnode->length = int_size_list[int_type] + 1;
    }
    else if ( int_type == XENAML_INT_WORD )
    {
        nnode->op = AML_WORD_OP;
        nnode->buffer[0] = AML_WORD_OP;
        xenaml_write_word(&ptr, (uint16_t)(value & 0xFFFF));
        nnode->length = int_size_list[int_type] + 1;
    }
    else if ( int_type == XENAML_INT_BYTE )
    {
        nnode->op = AML_BYTE_OP;
        nnode->buffer[0] = AML_BYTE_OP;
        nnode->buffer[1] = (uint8_t)(value & 0xFF);
        nnode->length = int_size_list[int_type] + 1;
    }
    else if ( int_type == XENAML_INT_ONES )
    {
        nnode->op = AML_ONES_OP;
        nnode->buffer[0] = AML_ONES_OP;
        nnode->length = int_size_list[int_type];
    }
    else if ( int_type == XENAML_INT_ONE )
    {
        nnode->op = AML_ONE_OP;
        nnode->buffer[0] = AML_ONE_OP;
        nnode->length = int_size_list[int_type];
    }
    else
    {
        nnode->op = AML_ZERO_OP;
        nnode->buffer[0] = AML_ZERO_OP;
        nnode->length = int_size_list[int_type];
    }

    return nnode;
}

EXTERNAL void*
xenaml_name_reference(const char *name, void *children, void *pma)
{
    struct xenaml_node *nnode;
    uint32_t nlength;

    /* See comments in xenaml_name_declaration about names. A name
     * reference is used in other constructs as arguments or parameters.
     * These names do not have a declaration op code and the op field
     * is left unassigned.
     */

    nlength = xenaml_scan_name(name);
    if ( nlength == 0 )
        return NULL;

    /* Make a new name node */
    nnode = xenaml_alloc_node(pma, nlength, 1);
    if ( nnode == NULL )
        return NULL;

    nnode->op = XENAML_UNASSIGNED_OPCODE;
    xenaml_format_name(name, &nnode->buffer[0], &nnode->flags);

    if ( children != NULL )
        xenaml_chain_children(nnode, children, NULL);

    return nnode;
}

EXTERNAL void*
xenaml_name_declaration(const char *name, void *init, void *pma)
{
    struct xenaml_node *nnode;
    uint32_t nlength;

    /* A name declaration is much like a name reference except it has the
     * AML_NAME_OP indicating it a declaration. The xenaml_build_name
     * routine contains comments on name formats. It also contains all the
     * logic to make a name.
     */

    nlength = xenaml_scan_name(name);
    if ( nlength == 0 )
        return NULL;

    /* Make a new name node with extra space for the decl. op */
    nnode = xenaml_alloc_node(pma, (nlength + 1), 1);
    if ( nnode == NULL )
        return NULL;

    nnode->op = AML_NAME_OP;
    nnode->buffer[0] = AML_NAME_OP;
    xenaml_format_name(name, &nnode->buffer[1], &nnode->flags);

    /* Caller requested name to be associated with an initial block as
     * first child
     */
    if ( init )
        xenaml_chain_children(nnode, init, NULL);

    return nnode;
}

EXTERNAL void*
xenaml_variable(enum xenaml_variable_type var_type,
                uint8_t var_num,
                void *pma)
{
    struct xenaml_node *nnode;

    switch (var_type) {
    case XENAML_VARIABLE_TYPE_LOCAL:
        if ( var_num > XENAML_MAX_VARIABLE_NUM )
            return NULL;
        break;
    case XENAML_VARIABLE_TYPE_ARG:
        if ( var_num > (XENAML_MAX_VARIABLE_NUM - 1) )
            return NULL;
        break;
    default:
        return NULL;
    };

    nnode = xenaml_alloc_node(pma, 1, 1);
    if ( nnode == NULL )
        return NULL;

    if ( var_type == XENAML_VARIABLE_TYPE_LOCAL )
    {
        nnode->op = local_op_list[var_num];
        nnode->buffer[0] = (uint8_t)(local_op_list[var_num] & 0xFF);
    }
    else
    {
        nnode->op = arg_op_list[var_num];
        nnode->buffer[0] = (uint8_t)(arg_op_list[var_num] & 0xFF);
    }

    return nnode;
}

EXTERNAL void*
xenaml_string(const char *str, void *pma)
{
    struct xenaml_node *nnode;
    uint32_t length;

    if ( str != NULL )
        length = strlen(str);
    else
        length = 0;

    nnode = xenaml_alloc_node(pma, (length + 2), 0);
    if ( nnode == NULL )
        return NULL;

    nnode->op = AML_STRING_OP;
    nnode->buffer[0] = AML_STRING_OP;
    if ( length > 0 )
        memcpy(&nnode->buffer[1], str, length);
    nnode->buffer[length + 1] = 0;

    return nnode;
}

EXTERNAL void*
xenaml_eisaid(const char *eisaid_str, void *pma)
{
    struct xenaml_node *nnode;
    uint32_t i, eisaid;

    /* Code taken from OpcDoEisaId in ACPI CA compiler code. See for more
     * details on the format.
     */

    /* The EISAID string must be exactly 7 characters and of the form
     * "UUUXXXX" -- 3 uppercase letters and 4 hex digits (e.g., "PNP0001")
     */
    if ( strlen(eisaid_str) != XENAML_EISAID_STR_LEN )
        return NULL;

    /* Check all 7 characters for correct format */
    for ( i = 0; i < XENAML_EISAID_STR_LEN; i++ )
    {
        if ( i < 3 )
        {
            /* First 3 characters must be uppercase letters */
            if ( !isupper((int)eisaid_str[i]) )
                return NULL;
        }
        else if ( !isxdigit((int)eisaid_str[i]) )
        {
            /* Last 4 characters must be hex digits */
            return NULL;
        }
    }

    eisaid = (uint32_t)((uint8_t)(eisaid_str[0] - 0x40)) << 26 |
             (uint32_t)((uint8_t)(eisaid_str[1] - 0x40)) << 21 |
             (uint32_t)((uint8_t)(eisaid_str[2] - 0x40)) << 16 |
             (xenaml_hex_char_to_hex(eisaid_str[3])) << 12 |
             (xenaml_hex_char_to_hex(eisaid_str[4])) << 8  |
             (xenaml_hex_char_to_hex(eisaid_str[5])) << 4  |
              xenaml_hex_char_to_hex(eisaid_str[6]);

    nnode = xenaml_integer(0, XENAML_INT_DWORD, pma);
    if ( nnode == NULL )
        return NULL;

    /* This one goes in endian backwards */
    nnode->buffer[1] = (eisaid >> 24) & 0xFF;
    nnode->buffer[2] = (eisaid >> 16) & 0xFF;
    nnode->buffer[3] = (eisaid >> 8) & 0xFF;
    nnode->buffer[4] = eisaid & 0xFF;

    return nnode;
}

EXTERNAL void*
xenaml_math(enum xenaml_math_func math_func,
            struct xenaml_args *arg_list,
            void *pma)
{
    struct xenaml_node *nnode;

    if ( math_func >= XENAML_MATH_FUNC_MAX || arg_list == NULL )
        return NULL;

    if ( !xenaml_check_args(math_op_list[math_func].count, arg_list) )
        return NULL;

    nnode = xenaml_alloc_node(pma, 1, 1);
    if ( nnode == NULL )
        return NULL;

    nnode->op = math_op_list[math_func].op;
    nnode->buffer[0] = (uint8_t)(math_op_list[math_func].op & 0xFF);

    xenaml_chain_args(nnode, arg_list);

    return nnode;
}

EXTERNAL void*
xenaml_logic(enum xenaml_logic_func logic_func,
             struct xenaml_args *arg_list,
             void *pma)
{
    struct xenaml_node *nnode;

    if ( logic_func >= XENAML_LOGIC_FUNC_MAX || arg_list == NULL )
        return NULL;

    if ( !xenaml_check_args(logic_op_list[logic_func].count, arg_list) )
        return NULL;

    nnode = xenaml_alloc_node(pma,
        (logic_op_list[logic_func].flags & XENAML_FLAG_DUAL_OP ? 2 : 1), 1);
    if ( nnode == NULL )
        return NULL;

    nnode->op = logic_op_list[logic_func].op;
    if ( logic_op_list[logic_func].flags & XENAML_FLAG_DUAL_OP )
    {
        nnode->buffer[0] = (uint8_t)((logic_op_list[logic_func].op >> 8) & 0xFF);
        nnode->buffer[1] = (uint8_t)(logic_op_list[logic_func].op & 0xFF);
    }
    else
        nnode->buffer[0] = (uint8_t)(logic_op_list[logic_func].op & 0xFF);

    xenaml_chain_args(nnode, arg_list);

    return nnode;
}

EXTERNAL void*
xenaml_misc(enum xenaml_misc_func misc_func,
            struct xenaml_args *arg_list,
            void *pma)
{
    struct xenaml_node *nnode;

    if ( misc_func >= XENAML_MISC_FUNC_MAX || arg_list == NULL )
        return NULL;

    if ( !xenaml_check_args(misc_op_list[misc_func].count, arg_list) )
        return NULL;

    if ( misc_op_list[misc_func].flags & XENAML_FLAG_NAME_ARGS_ONLY )
    {
        if ( (((struct xenaml_node*)arg_list->arg[0])->op & XENAML_NAME_OP)
            == 0 )
            return NULL;
        if ( (((struct xenaml_node*)arg_list->arg[1])->op & XENAML_NAME_OP)
            == 0 )
            return NULL;
    }

    nnode = xenaml_alloc_node(pma,
        (misc_op_list[misc_func].flags & XENAML_FLAG_DUAL_OP ? 2 : 1), 1);
    if ( nnode == NULL )
        return NULL;

    nnode->op = misc_op_list[misc_func].op;
    if ( misc_op_list[misc_func].flags & XENAML_FLAG_DUAL_OP )
    {
        nnode->buffer[0] = (uint8_t)((misc_op_list[misc_func].op >> 8) & 0xFF);
        nnode->buffer[1] = (uint8_t)(misc_op_list[misc_func].op & 0xFF);
    }
    else
        nnode->buffer[0] = (uint8_t)(misc_op_list[misc_func].op & 0xFF);

    xenaml_chain_args(nnode, arg_list);

    return nnode;
}

EXTERNAL void*
xenaml_create_field(enum xenaml_create_field create_field,
                    const char *field_name,
                    const char *source_buffer,
                    uint32_t bit_byte_index,
                    uint32_t num_bits,
                    void *pma)
{
    struct xenaml_node *nnode = NULL, *snode = NULL;
    struct xenaml_node *inode = NULL, *bnode = NULL, *cnode = NULL;
    xenaml_bool is_cf = (create_field == XENAML_CREATE_FIELD) ? 1 : 0;

    if ( field_name == NULL || source_buffer == NULL )
        return NULL;

    /* Make an index integer node and optionally a number of bits node */
    inode = xenaml_integer(bit_byte_index, XENAML_INT_OPTIMIZE, pma);
    if ( inode == NULL )
        return NULL;

    if ( is_cf )
    {
        bnode = xenaml_integer(num_bits, XENAML_INT_OPTIMIZE, pma);
        if ( bnode == NULL )
            goto err_out;
    }

    /* Make a name node for the source buffer name */
    snode = xenaml_name_reference(source_buffer, NULL, pma);
    if ( snode == NULL )
        goto err_out;

    /* Make a name node for the field name */
    nnode = xenaml_name_reference(field_name, NULL, pma);
    if ( nnode == NULL )
        goto err_out;

    /* Make a create op node, no package len block */
    cnode = xenaml_alloc_node(pma, (is_cf ? 2 : 1), 0);
    if ( cnode == NULL )
        goto err_out;
    cnode->op = create_field_op_list[create_field].op;
    if ( is_cf )
    {
        cnode->buffer[0] =
            (uint8_t)((create_field_op_list[create_field].op >> 8) & 0xFF);
        cnode->buffer[1] =
            (uint8_t)(create_field_op_list[create_field].op & 0xFF);
    }
    else
    {
        cnode->buffer[0] =
            (uint8_t)(create_field_op_list[create_field].op & 0xFF);
    }

    /* Chain it all together */
    xenaml_chain_peers(snode, inode, NULL);
    if ( is_cf )
    {
        xenaml_chain_peers(inode, bnode, NULL);
        xenaml_chain_peers(nnode, nnode, NULL);
    }
    else
        xenaml_chain_peers(inode, nnode, NULL);

    xenaml_chain_children(cnode, snode, NULL);

    return cnode;
err_out:
    if ( pma == NULL )
    {
        if ( inode != NULL )
            free(inode);
        if ( bnode != NULL )
            free(bnode);
        if ( nnode != NULL )
            free(nnode);
        if ( snode != NULL )
            free(snode);
    } /* else SNO */

    return NULL;
}

EXTERNAL void*
xenaml_if(void *predicate, void *term_list, void *pma)
{
    struct xenaml_node *inode;
    uint8_t pkg_len_buf[4];
    uint32_t total = 0, pkg_len_size;

    if ( predicate == NULL || term_list == NULL )
        return NULL;

    /* Need to calculate the size of all the children to fill in
     * pkg len. Both predicate and term_list could be trees with many nodes.
     */
    xenaml_calculate_length(predicate, &total);
    xenaml_calculate_length(term_list, &total);

    /* Now the package length size can be computed */
    pkg_len_size = xenaml_package_length(&pkg_len_buf[0],
                                         total);

    /* Total length is the if op + pkg len. The predicate and term_list
     * are chained on.
     */
    inode = xenaml_alloc_node(pma, (1 + pkg_len_size), 0);
    if ( inode == NULL )
        return NULL;
    inode->op = AML_IF_OP;
    inode->flags = XENAML_FLAG_NODE_PACKAGE;
    inode->buffer[0] = AML_IF_OP;
    memcpy(&inode->buffer[1], &pkg_len_buf[0], pkg_len_size);

    /* Chain the predicate and term_list on as children of the if op */
    xenaml_chain_children(inode, predicate, NULL);
    xenaml_chain_peers(predicate, term_list, NULL);

    return inode;
}

EXTERNAL void*
xenaml_else(void *term_list, void *pma)
{
    struct xenaml_node *enode;
    uint8_t pkg_len_buf[4];
    uint32_t total = 0, pkg_len_size;

    if ( term_list == NULL )
        return NULL;

    /* Need to calculate the size of all the children to fill in
     * pkg len. Both term_list could be a tree with many nodes.
     */
    xenaml_calculate_length(term_list, &total);

    /* Now the package length size can be computed */
    pkg_len_size = xenaml_package_length(&pkg_len_buf[0],
                                         total);

    /* Total length is the if op + pkg len. The term_list is chained on. */
    enode = xenaml_alloc_node(pma, (1 + pkg_len_size), 0);
    if ( enode == NULL )
        return NULL;
    enode->op = AML_ELSE_OP;
    enode->flags = XENAML_FLAG_NODE_PACKAGE;
    enode->buffer[0] = AML_ELSE_OP;
    memcpy(&enode->buffer[1], &pkg_len_buf[0], pkg_len_size);

    /* Chain the term_list on as child of the else op */
    xenaml_chain_children(enode, term_list, NULL);

    return enode;
}

EXTERNAL void*
xenaml_while(void *predicate, void *term_list, void *pma)
{
    struct xenaml_node *wnode;
    uint8_t pkg_len_buf[4];
    uint32_t total = 0, pkg_len_size;

    if ( predicate == NULL || term_list == NULL )
        return NULL;

    /* Need to calculate the size of all the children to fill in
     * pkg len. Both predicate and term_list could be trees with many nodes.
     */
    xenaml_calculate_length(predicate, &total);
    xenaml_calculate_length(term_list, &total);

    /* Now the package length size can be computed */
    pkg_len_size = xenaml_package_length(&pkg_len_buf[0],
                                         total);

    /* Total length is the if op + pkg len. The predicate and term_list
     * are chained on.
     */
    wnode = xenaml_alloc_node(pma, (1 + pkg_len_size), 0);
    if ( wnode == NULL )
        return NULL;
    wnode->op = AML_WHILE_OP;
    wnode->flags = XENAML_FLAG_NODE_PACKAGE;
    wnode->buffer[0] = AML_WHILE_OP;
    memcpy(&wnode->buffer[1], &pkg_len_buf[0], pkg_len_size);

    /* Chain the predicate and term_list on as children of the while op */
    xenaml_chain_children(wnode, predicate, NULL);
    xenaml_chain_peers(predicate, term_list, NULL);

    return wnode;
}

EXTERNAL void*
xenaml_buffer(struct xenaml_buffer_init *buffer_init, void *pma)
{
    struct xenaml_node *bnode, *lnode, *rnode;
    uint8_t pkg_len_buf[4];
    uint32_t length, pkg_len_size;

    if ( (buffer_init == NULL)||
         (buffer_init->init_type > XENAML_BUFFER_INIT_NAMELEN) )
        return NULL;

    /* First have to sort out if the caller wants a buffer initialized with
     * raw data or an uninitialized new buffer. This will determine the layout
     * of the buffer AML.
     */
    if ( buffer_init->init_type == XENAML_BUFFER_INIT_RAWDATA )
    {
        if ( buffer_init->aml_buffer.aml_rawdata.buffer == NULL ||
             buffer_init->aml_buffer.aml_rawdata.raw_length == 0 )
            return NULL;
        /* Create a length specifier and the raw data buffer as children */
        lnode = xenaml_integer(buffer_init->aml_buffer.aml_rawdata.raw_length,
                               XENAML_INT_OPTIMIZE,
                               pma);
        if ( lnode == NULL )
            return NULL;
        rnode = xenaml_raw_data(buffer_init->aml_buffer.aml_rawdata.buffer,
                                buffer_init->aml_buffer.aml_rawdata.raw_length,
                                pma);
        if ( rnode == NULL )
        {
            free(lnode);
            return NULL;
        }
        length = lnode->length + rnode->length;
        xenaml_chain_peers(lnode, rnode, NULL);
    }
    else if ( buffer_init->init_type == XENAML_BUFFER_INIT_VARLEN )
    {
        /* Create a variable node to specify the uninitialized length, the
         * xenaml_variable() call will do the input checking.
         */
        lnode = xenaml_variable(buffer_init->aml_buffer.aml_varlen.var_type,
                                buffer_init->aml_buffer.aml_varlen.var_num,
                                pma);
        if ( lnode == NULL )
            return NULL;
        length = lnode->length;
    }
    else if ( buffer_init->init_type == XENAML_BUFFER_INIT_INTLEN )
    {
        if ( buffer_init->aml_buffer.aml_intlen.length == 0 )
            return NULL; /* not supported and makes iasl moan */
        /* Create an integer node to specify the buffer length directly */
        lnode = xenaml_integer(buffer_init->aml_buffer.aml_intlen.length,
                               XENAML_INT_OPTIMIZE,
                               pma);
        if ( lnode == NULL )
            return NULL;
        length = lnode->length;
    }
    else
    {
        /* buffer_init->init_type == XENAML_BUFFER_INIT_NAMELEN */
        /* Create a node to specify the buffer length via some named values */
        lnode = xenaml_name_reference(buffer_init->aml_buffer.aml_namelen.name,
                                      NULL,
                                      pma);
        if ( lnode == NULL )
            return NULL;
        length = lnode->length;
    }

    /* Determine the package length based off the size of the child node(s) */
    pkg_len_size = xenaml_package_length(&pkg_len_buf[0],
                                         length);

    /* Total length is the buffer op + pkg len */
    bnode = xenaml_alloc_node(pma, (1 + pkg_len_size), 1);
    if ( bnode == NULL )
    {
        if ( pma == NULL )
            xenaml_delete_list(lnode);
        return NULL;
    }
    bnode->op = AML_BUFFER_OP;
    bnode->flags = XENAML_FLAG_NODE_PACKAGE;
    bnode->buffer[0] = AML_BUFFER_OP;
    memcpy(&bnode->buffer[1], &pkg_len_buf[0], pkg_len_size);

    /* Chain the inner node(s) on as children of the buffer */
    xenaml_chain_children(bnode, lnode, NULL);

    return bnode;
}

EXTERNAL void*
xenaml_package(uint32_t num_elements, void *package_list, void *pma)
{
    struct xenaml_node *pnode, *enode;
    uint8_t pkg_len_buf[4], rawb;
    uint32_t count = 0, total = 0;
    uint32_t pkg_len_size;

    if (!xenaml_check_package_elements(package_list, &count))
        return NULL;

    /* Note that package_list may be NULL indicating there is no initializer
     * list. In this case, a count must be passed in to initialize the
     * package.
     */
    if ( package_list == NULL )
    {
        if ( num_elements == 0 )
            return NULL;
        count = num_elements;
    }

    /* Need to calculate the size of all the children to fill in pkg len. */
    if ( package_list != NULL )
        xenaml_calculate_length(package_list, &total);

    /* Now the element count field must be created based on the number of
     * package_list items.
     */
    if ( count <= XENAML_MAX_PACKAGE_ELEMS )
    {
        /* Bit of a trick, use a raw buffer to give us a single byte node */
        rawb = (uint8_t)(count & 0xFF);
        enode = xenaml_raw_data(&rawb, 1, pma);
    }
    else
        enode = xenaml_integer(count, XENAML_INT_OPTIMIZE, pma);

    if ( enode == NULL )
        return NULL;

    total += enode->length;

    /* Now the package length size can be computed. */
    pkg_len_size = xenaml_package_length(&pkg_len_buf[0],
                                         total);

    /* Total length is the if op + pkg len. The element
     * count field and the package_list are chained on.
     */
    pnode = xenaml_alloc_node(pma, (1 + pkg_len_size), 0);
    if ( pnode == NULL )
    {
        if ( pma == NULL )
            free(enode);
        return NULL;
    }
    pnode->op = ( count <= XENAML_MAX_PACKAGE_ELEMS ) ?
                    AML_PACKAGE_OP : AML_VAR_PACKAGE_OP;
    pnode->flags = XENAML_FLAG_NODE_PACKAGE;
    pnode->buffer[0] = ( count <= XENAML_MAX_PACKAGE_ELEMS ) ?
                          AML_PACKAGE_OP : AML_VAR_PACKAGE_OP;
    memcpy(&pnode->buffer[1], &pkg_len_buf[0], pkg_len_size);

    /* Chain on the count specifier and optionally any
     * initialization package_list.
     */
    xenaml_chain_children(pnode, enode, NULL);
    if ( package_list != NULL )
        xenaml_chain_peers(enode, package_list, NULL);

    return pnode;
}

EXTERNAL void*
xenaml_mutex(const char *mutex_name,
             uint8_t sync_level,
             void *pma)
{
    struct xenaml_node *mnode;
    uint32_t nlength;

    if ( sync_level > XENAML_MAX_SYNC_LEVEL )
        return NULL;

    nlength = xenaml_scan_name(mutex_name);
    if ( nlength == 0 )
        return NULL;

    /* Total length is the if dual-op + name + 1 byte param. */
    mnode = xenaml_alloc_node(pma, (2 + nlength + 1), 0);
    if ( mnode == NULL )
        return NULL;
    mnode->op = AML_MUTEX_OP;
    mnode->flags = XENAML_FLAG_DUAL_OP;
    mnode->buffer[0] = (AML_MUTEX_OP >> 8) & 0xFF;
    mnode->buffer[1] = (AML_MUTEX_OP) & 0xFF;
    xenaml_format_name(mutex_name, &mnode->buffer[2], &mnode->flags);
    mnode->buffer[nlength + 2] = sync_level;

    return mnode;
}

EXTERNAL void*
xenaml_acquire(const char *sync_object,
               uint16_t timout_value,
               void *pma)
{
    struct xenaml_node *anode;
    uint32_t nlength;
    uint8_t *ptr;

    nlength = xenaml_scan_name(sync_object);
    if ( nlength == 0 )
        return NULL;

    /* Total length is the if dual-op + name + 1 param word. */
    anode = xenaml_alloc_node(pma, (2 + nlength + 2), 0);
    if ( anode == NULL )
        return NULL;
    anode->op = AML_ACQUIRE_OP;
    anode->flags = XENAML_FLAG_DUAL_OP;
    anode->buffer[0] = (AML_ACQUIRE_OP >> 8) & 0xFF;
    anode->buffer[1] = (AML_ACQUIRE_OP) & 0xFF;
    xenaml_format_name(sync_object, &anode->buffer[2], &anode->flags);
    ptr = anode->buffer + nlength + 2;
    xenaml_write_word(&ptr, timout_value);

    return anode;
}

EXTERNAL void*
xenaml_release(const char *sync_object,
               void *pma)
{
    struct xenaml_node *rnode;
    uint32_t nlength;

    nlength = xenaml_scan_name(sync_object);
    if ( nlength == 0 )
        return NULL;

    /* Total length is the if dual-op + name. */
    rnode = xenaml_alloc_node(pma, (2 + nlength), 0);
    if ( rnode == NULL )
        return NULL;
    rnode->op = AML_RELEASE_OP;
    rnode->flags = XENAML_FLAG_DUAL_OP;
    rnode->buffer[0] = (AML_RELEASE_OP >> 8) & 0xFF;
    rnode->buffer[1] = (AML_RELEASE_OP) & 0xFF;
    xenaml_format_name(sync_object, &rnode->buffer[2], &rnode->flags);

    return rnode;
}

EXTERNAL void*
xenaml_event(const char *event_name,
             void *pma)
{
    struct xenaml_node *enode;
    uint32_t nlength;

    nlength = xenaml_scan_name(event_name);
    if ( nlength == 0 )
        return NULL;

    /* Total length is the if dual-op + name. */
    enode = xenaml_alloc_node(pma, (2 + nlength), 0);
    if ( enode == NULL )
        return NULL;
    enode->op = AML_EVENT_OP;
    enode->flags = XENAML_FLAG_DUAL_OP;
    enode->buffer[0] = (AML_EVENT_OP >> 8) & 0xFF;
    enode->buffer[1] = (AML_EVENT_OP) & 0xFF;
    xenaml_format_name(event_name, &enode->buffer[2], &enode->flags);

    return enode;
}

EXTERNAL void*
xenaml_wait(const char *sync_object,
            uint16_t timout_value,
            void *pma)
{
    struct xenaml_node *wnode, *tnode;
    uint32_t nlength;

    nlength = xenaml_scan_name(sync_object);
    if ( nlength == 0 )
        return NULL;

    /* This one stores the wait value in a separate integer
     * op node.
     */
    tnode = xenaml_integer(timout_value, XENAML_INT_OPTIMIZE, pma);
    if ( tnode == NULL )
        return NULL;

    /* Total length is the if dual-op + name. */
    wnode = xenaml_alloc_node(pma, (2 + nlength), 0);
    if ( wnode == NULL )
        goto err_out;
    wnode->op = AML_WAIT_OP;
    wnode->flags = XENAML_FLAG_DUAL_OP;
    wnode->buffer[0] = (AML_WAIT_OP >> 8) & 0xFF;
    wnode->buffer[1] = (AML_WAIT_OP) & 0xFF;
    xenaml_format_name(sync_object, &wnode->buffer[2], &wnode->flags);

    xenaml_chain_children(wnode, tnode, NULL);

    return wnode;

err_out:
    if ( pma == NULL )
    {
        if ( tnode != NULL )
            free(tnode);
        if ( wnode != NULL )
            free(wnode);
    } /* else SNO */

    return NULL;
}

EXTERNAL void*
xenaml_signal(const char *sync_object,
              void *pma)
{
    struct xenaml_node *snode;
    uint32_t nlength;

    nlength = xenaml_scan_name(sync_object);
    if ( nlength == 0 )
        return NULL;

    /* Total length is the if dual-op + name. */
    snode = xenaml_alloc_node(pma, (2 + nlength), 0);
    if ( snode == NULL )
        return NULL;
    snode->op = AML_SIGNAL_OP;
    snode->flags = XENAML_FLAG_DUAL_OP;
    snode->buffer[0] = (AML_SIGNAL_OP >> 8) & 0xFF;
    snode->buffer[1] = (AML_SIGNAL_OP) & 0xFF;
    xenaml_format_name(sync_object, &snode->buffer[2], &snode->flags);

    return snode;
}

EXTERNAL void*
xenaml_reset(const char *sync_object,
             void *pma)
{
    struct xenaml_node *rnode;
    uint32_t nlength;

    nlength = xenaml_scan_name(sync_object);
    if ( nlength == 0 )
        return NULL;

    /* Total length is the if dual-op + name. */
    rnode = xenaml_alloc_node(pma, (2 + nlength), 0);
    if ( rnode == NULL )
        return NULL;
    rnode->op = AML_RESET_OP;
    rnode->flags = XENAML_FLAG_DUAL_OP;
    rnode->buffer[0] = (AML_RESET_OP >> 8) & 0xFF;
    rnode->buffer[1] = (AML_RESET_OP) & 0xFF;
    xenaml_format_name(sync_object, &rnode->buffer[2], &rnode->flags);

    return rnode;
}

EXTERNAL void*
xenaml_power_resource(const char *resource_name,
                      uint8_t system_level,
                      uint16_t resource_order,
                      void *object_list,
                      void *pma)
{
    struct xenaml_node *pnode;
    uint8_t pkg_len_buf[4];
    uint32_t nlength, total = 0, pkg_len_size;
    uint8_t *ptr;

    /* Power res needs to have a object_list with at least
     * _STA/_ON/_OFF
     */
    if ( object_list == NULL )
        return NULL;

    nlength = xenaml_scan_name(resource_name);
    if ( nlength == 0 )
        return NULL;

    /* Need to calculate the size of all the children to fill in
     * pkg len. The children are the entire object_list of the power resource.
     */
    xenaml_calculate_length(object_list, &total);

    /* Now the package length size can be computed. Have to include
     * the name + 3 param bytes
     */
    pkg_len_size = xenaml_package_length(&pkg_len_buf[0],
                                         (total + nlength + 3));

    /* Total length is the if dual-op + pkg len + name + 3 param bytes. */
    pnode = xenaml_alloc_node(pma, (2 + pkg_len_size + nlength + 3), 0);
    if ( pnode == NULL )
        return NULL;
    pnode->op = AML_POWER_RES_OP;
    pnode->flags = XENAML_FLAG_NODE_PACKAGE|XENAML_FLAG_DUAL_OP;
    pnode->buffer[0] = (AML_POWER_RES_OP >> 8) & 0xFF;
    pnode->buffer[1] = (AML_POWER_RES_OP) & 0xFF;
    memcpy(&pnode->buffer[2], &pkg_len_buf[0], pkg_len_size);
    xenaml_format_name(resource_name, &pnode->buffer[pkg_len_size + 2], &pnode->flags);
    ptr = pnode->buffer + pkg_len_size + nlength + 2;
    xenaml_write_byte(&ptr, system_level);
    xenaml_write_word(&ptr, resource_order);

    /* Chain the object_list on as child of the method op if present. */
    xenaml_chain_children(pnode, object_list, NULL);

    return pnode;
}

EXTERNAL void*
xenaml_thermal_zone(const char *thermal_zone_name,
                    void *object_list,
                    void *pma)
{
    struct xenaml_node *tnode;
    uint8_t pkg_len_buf[4];
    uint32_t nlength, total = 0, pkg_len_size;

    /* Power res needs to have a object_list with at least
     * _TMP/_CRT
     */
    if ( object_list == NULL )
        return NULL;

    nlength = xenaml_scan_name(thermal_zone_name);
    if ( nlength == 0 )
        return NULL;

    /* Need to calculate the size of all the children to fill in
     * pkg len. The children are the entire object_list of the power resource.
     */
    xenaml_calculate_length(object_list, &total);

    /* Now the package length size can be computed with name */
    pkg_len_size = xenaml_package_length(&pkg_len_buf[0],
                                         (total + nlength));

    /* Total length is the if dual-op + pkg len + name. */
    tnode = xenaml_alloc_node(pma, (2 + pkg_len_size + nlength), 0);
    if ( tnode == NULL )
        return NULL;
    tnode->op = AML_THERMAL_ZONE_OP;
    tnode->flags = XENAML_FLAG_NODE_PACKAGE|XENAML_FLAG_DUAL_OP;
    tnode->buffer[0] = (AML_THERMAL_ZONE_OP >> 8) & 0xFF;
    tnode->buffer[1] = (AML_THERMAL_ZONE_OP) & 0xFF;
    memcpy(&tnode->buffer[2], &pkg_len_buf[0], pkg_len_size);
    xenaml_format_name(thermal_zone_name, &tnode->buffer[pkg_len_size + 2], &tnode->flags);

    /* Chain the object_list on as child of the method op if present. */
    xenaml_chain_children(tnode, object_list, NULL);

    return tnode;
}

EXTERNAL void*
xenaml_processor(const char *processor_name,
                 uint8_t processor_id,
                 uint32_t pblock_addr,
                 uint8_t pblock_length,
                 void *object_list,
                 void *pma)
{
    struct xenaml_node *pnode;
    uint8_t pkg_len_buf[4];
    uint32_t nlength, total = 0, pkg_len_size;
    uint8_t *ptr;

    /* object_list can be empty and usually is */

    if ( (pblock_length != 0)&&(pblock_length != 6) )
        return NULL;

    nlength = xenaml_scan_name(processor_name);
    if ( nlength == 0 )
        return NULL;

    /* Need to calculate the size of all the children to fill in
     * pkg len though object_list is most likely empty.
     */
    if ( object_list != NULL )
        xenaml_calculate_length(object_list, &total);

    /* Now the package length size can be computed. Have to include
     * the name + 6 param bytes
     */
    pkg_len_size = xenaml_package_length(&pkg_len_buf[0],
                                         (total + nlength + 6));

    /* Total length is the if dual-op + pkg len + name + 6 param bytes. */
    pnode = xenaml_alloc_node(pma, (2 + pkg_len_size + nlength + 6), 0);
    if ( pnode == NULL )
        return NULL;
    pnode->op = AML_PROCESSOR_OP;
    pnode->flags = XENAML_FLAG_NODE_PACKAGE|XENAML_FLAG_DUAL_OP;
    pnode->buffer[0] = (AML_PROCESSOR_OP >> 8) & 0xFF;
    pnode->buffer[1] = (AML_PROCESSOR_OP) & 0xFF;
    memcpy(&pnode->buffer[2], &pkg_len_buf[0], pkg_len_size);
    xenaml_format_name(processor_name, &pnode->buffer[pkg_len_size + 2], &pnode->flags);
    ptr = pnode->buffer + pkg_len_size + nlength + 2;
    xenaml_write_byte(&ptr, processor_id);
    xenaml_write_dword(&ptr, pblock_addr);
    xenaml_write_byte(&ptr, pblock_length);

    /* Chain the object_list on as child of the method op if present. */
    if ( object_list != NULL )
        xenaml_chain_children(pnode, object_list, NULL);

    return pnode;
}

EXTERNAL void*
xenaml_method(const char *method_name,
              uint8_t num_args,
              xenaml_bool serialized,
              void *term_list,
              void *pma)
{
    struct xenaml_node *mnode;
    uint8_t pkg_len_buf[4];
    uint32_t nlength, total = 0, pkg_len_size;

    if ( num_args > (XENAML_MAX_ARGUMENT_NUM + 1) )
        return NULL;

    nlength = xenaml_scan_name(method_name);
    if ( nlength == 0 )
        return NULL;

    /* Need to calculate the size of all the children to fill in
     * pkg len. The children are the entire term_list of the method that the
     * caller formed.
     */
    xenaml_calculate_length(term_list, &total);

    /* Now the package length size can be computed. Have to include
     * the name + attrs byte
     */
    pkg_len_size = xenaml_package_length(&pkg_len_buf[0],
                                         (total + nlength + 1));

    /* Total length is the if op + pkg len + name + attrs byte. */
    mnode = xenaml_alloc_node(pma, (1 + pkg_len_size + nlength + 1), 0);
    if ( mnode == NULL )
        return NULL;
    mnode->op = AML_METHOD_OP;
    mnode->flags = XENAML_FLAG_NODE_PACKAGE;
    mnode->buffer[0] = AML_METHOD_OP;
    memcpy(&mnode->buffer[1], &pkg_len_buf[0], pkg_len_size);
    xenaml_format_name(method_name, &mnode->buffer[pkg_len_size + 1], &mnode->flags);
    mnode->buffer[pkg_len_size + nlength + 1] = num_args;
    if ( serialized )
        mnode->buffer[pkg_len_size + nlength + 1] |= AML_METHOD_SERIALIZED;

    /* Chain the term_list node on as child of the method op if present. In
     * theory term_list could be empty if you had an unimplemented method
     */
    if ( term_list != NULL )
        xenaml_chain_children(mnode, term_list, NULL);

    return mnode;
}

EXTERNAL void*
xenaml_op_region(const char *region_name,
                 uint8_t region_space,
                 uint64_t region_offset,
                 uint64_t region_length,
                 void *pma)
{
    struct xenaml_node *onode;
    struct xenaml_node *tnode = NULL, *anode = NULL, *snode = NULL;
    uint8_t acpi_addr_type;
    uint32_t nlength;

    nlength = xenaml_scan_name(region_name);
    if ( nlength == 0 )
        return NULL;

    acpi_addr_type = xenaml_op_region_values(region_space,
                                             region_offset,
                                             region_length,
                                             pma,
                                             &anode);
    if ( acpi_addr_type == XENAML_ADR_SPACE_INVALID )
        goto err_out;

    /* Make integer nodes for the type and size */
    tnode = xenaml_integer(acpi_addr_type, XENAML_INT_OPTIMIZE, pma);
    snode = xenaml_integer(region_length, XENAML_INT_OPTIMIZE, pma);
    if ( tnode == NULL || snode == NULL )
        goto err_out;

    /* Create the parent op region node + name and chain the kids on */
    onode = xenaml_alloc_node(pma, (2 + nlength), 1);
    if ( onode == NULL )
        goto err_out;
    onode->op = AML_REGION_OP;
    onode->flags = XENAML_FLAG_DUAL_OP;
    onode->buffer[0] = (AML_REGION_OP >> 8) & 0xFF;
    onode->buffer[1] = (AML_REGION_OP) & 0xFF;
    xenaml_format_name(region_name, &onode->buffer[2], &onode->flags);

    xenaml_chain_children(onode, tnode, NULL);
    xenaml_chain_peers(tnode, anode, NULL);
    xenaml_chain_peers(anode, snode, NULL);

    return onode;
err_out:
    if ( pma == NULL )
    {
        if ( anode != NULL )
            free(anode);
        if ( snode != NULL )
            free(snode);
        if ( tnode != NULL )
            free(tnode);
    } /* esle SNO */

    return NULL;
}

EXTERNAL void*
xenaml_field(const char *region_name,
             enum xenaml_field_acccess_type access_type,
             enum xenaml_field_lock_rule lock_rule,
             enum xenaml_field_update_rule update_rule,
             struct xenaml_field_unit *field_unit_list,
             uint32_t unit_count,
             void *pma)
{
    struct xenaml_node *fnode;
    uint32_t i, nlength, total = 0;
    uint8_t pkg_len_buf[4];
    uint32_t pkg_len_size;
    uint8_t *ptr;
    uint8_t acpi_field_flags;

    nlength = xenaml_scan_name(region_name);
    if ( nlength == 0 )
        return NULL;

    if ( field_unit_list == NULL || unit_count == 0 )
        return NULL;

    if ( access_type > XENAML_FIELD_ACCESS_TYPE_BUFFER )
        return NULL;

    if ( (update_rule != XENAML_FIELD_UPDATE_PRESERVE)&&
         (update_rule != XENAML_FIELD_UPDATE_WRITEASONES)&&
         (update_rule != XENAML_FIELD_UPDATE_WRITEASZEROES) )
        return NULL;

    /* The XENAML field access values, lock rules and update rules map right
     * to the ones from ACPICA.
     */
    acpi_field_flags = access_type|lock_rule|update_rule;

    /* Calculate the size needed for all the field definitions. */
    for ( i = 0; i < unit_count; i++ )
    {
        if ( field_unit_list[i].type == XENAML_FIELD_TYPE_NAME )
            total += (ACPI_NAME_SIZE + 1);
        else
            total += 2; /* offset indicator byte 0x00 + the offset value */
    }
    total += (nlength + 1); /* add for the field op name and flags byte */

    /* package length size computed */
    pkg_len_size = xenaml_package_length(&pkg_len_buf[0],
                                         total);

    /* Total length is the field op + pkg len + field op name + n*field
     * values.
     */
    fnode = xenaml_alloc_node(pma, (2 + pkg_len_size + total), 0);
    if ( fnode == NULL )
        return NULL;
    fnode->op = AML_FIELD_OP;
    fnode->flags = XENAML_FLAG_NODE_PACKAGE|XENAML_FLAG_DUAL_OP;
    fnode->buffer[0] = (AML_FIELD_OP >> 8) & 0xFF;
    fnode->buffer[1] = (AML_FIELD_OP) & 0xFF;
    memcpy(&fnode->buffer[2], &pkg_len_buf[0], pkg_len_size);
    ptr = fnode->buffer + 2 + pkg_len_size;
    xenaml_format_name(region_name, ptr, &fnode->flags);
    ptr += nlength;
    *ptr++ = acpi_field_flags;

    /* Loop over all the field definitions, adding each */
    for ( i = 0; i < unit_count; i++ )
    {
        if ( field_unit_list[i].type == XENAML_FIELD_TYPE_NAME )
        {
            memcpy(ptr,
                   &field_unit_list[i].aml_field.aml_name.name[0],
                   ACPI_NAME_SIZE);
            ptr += ACPI_NAME_SIZE;
            *ptr++ = field_unit_list[i].aml_field.aml_name.size_in_bits;
        }
        else
        {
            *ptr++ = XENAML_FIELD_OFFSET_IND;
            *ptr++ = field_unit_list[i].aml_field.aml_offset.bits_to_offset;
        }
    }

    return fnode;
}

EXTERNAL void*
xenaml_device(const char *device_name, void *object_list, void *pma)
{
    struct xenaml_node *dnode;
    uint32_t nlength, total = 0;
    uint8_t pkg_len_buf[4];
    uint32_t pkg_len_size;

    /* A device must have a name and child nodes */
    if ( object_list == NULL )
        return NULL;

    nlength = xenaml_scan_name(device_name);
    if ( nlength == 0 )
        return NULL;

    /* Need to calculate the size of all the object_list to fill in
     * pkg len. The object_list are all the methods, names, packages etc that
     * make up a device.
     */
    xenaml_calculate_length(object_list, &total);

    /* Package length size computed - include the device name. */
    pkg_len_size = xenaml_package_length(&pkg_len_buf[0],
                                         (total + nlength));

    /* Total length is the if dual-op + pkg len + device name. */
    dnode = xenaml_alloc_node(pma, (2 + pkg_len_size + nlength), 0);
    if ( dnode == NULL )
        return NULL;
    dnode->op = AML_DEVICE_OP;
    dnode->flags = XENAML_FLAG_NODE_PACKAGE|XENAML_FLAG_DUAL_OP;
    dnode->buffer[0] = (AML_DEVICE_OP >> 8) & 0xFF;
    dnode->buffer[1] = (AML_DEVICE_OP) & 0xFF;
    memcpy(&dnode->buffer[2], &pkg_len_buf[0], pkg_len_size);
    xenaml_format_name(device_name, &dnode->buffer[pkg_len_size + 2], &dnode->flags);

    /* Chain the child list on as child of the device op. */
    xenaml_chain_children(dnode, object_list, NULL);

    return dnode;
}

EXTERNAL void*
xenaml_scope(const char *name, void *object_list, void *pma)
{
    struct xenaml_node *snode;
    uint32_t total = 0;
    uint8_t pkg_len_buf[4];
    uint32_t pkg_len_size;

    /* A scope must have a name and child nodes. I guess you could make
     * an empty scope but what would be the purpose. Don't waste my time.
     */
    if ( name == NULL || object_list == NULL )
        return NULL;

    /* Scope names are fixed at \NNNN */
    if ( strlen(name) != (ACPI_NAME_SIZE + 1) )
        return NULL;

    if ( name[0] != AML_ROOT_PREFIX )
        return NULL;

    /* Need to calculate the size of all the object_list to fill in
     * pkg len. The object_list are basically everything you created before
     * this point.
     */
    xenaml_calculate_length(object_list, &total);

    /* Package length size computed - have to include the name + root char. */
    pkg_len_size = xenaml_package_length(&pkg_len_buf[0],
                                         (total + ACPI_NAME_SIZE + 1));

    /* Total length is the if op + pkg len + root-byte + name. */
    snode = xenaml_alloc_node(pma, (1 + pkg_len_size + 1 + ACPI_NAME_SIZE), 0);
    if ( snode == NULL )
        return NULL;
    snode->op = AML_SCOPE_OP;
    snode->flags = XENAML_FLAG_NODE_PACKAGE;
    snode->buffer[0] = AML_SCOPE_OP;
    memcpy(&snode->buffer[1], &pkg_len_buf[0], pkg_len_size);
    memcpy(&snode->buffer[pkg_len_size + 1], &name[0], (ACPI_NAME_SIZE + 1));

    /* Chain the child list on as child of the scope op. */
    xenaml_chain_children(snode, object_list, NULL);

    return snode;
}


