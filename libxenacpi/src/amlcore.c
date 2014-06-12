/*
 * amlcore.c
 *
 * XEN ACPI AML code support code.
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

static uint32_t sc_pagesize = 0;

void*
xenaml_prealloc(struct xenaml_premem *premem,
                uint32_t length)
{
    uint32_t length_aligned = XENAML_MASK_ALIGN(length);
    void *outp;

    if ( length_aligned > premem->free )
        return NULL;

    outp = premem->next;
    premem->free -= length_aligned;
    premem->next += length_aligned;

    return outp;
}

void*
xenaml_alloc_node(struct xenaml_premem *premem,
                  uint32_t length,
                  xenaml_bool zero_buf)
{
    struct xenaml_node *node;
    uint32_t total = sizeof(struct xenaml_node) + length;

    if ( premem != NULL )
        node = xenaml_prealloc(premem, total);
    else
        node = malloc(total);

    if ( node == NULL )
        return NULL;
    if ( zero_buf )
        memset(node, 0, total);
    else
        memset(node, 0, sizeof(struct xenaml_node));

    node->buffer = ((uint8_t*)node) + sizeof(struct xenaml_node);
    node->length = length;
    if ( premem != NULL )
        node->flags |= XENAML_FLAG_PREMEM_ALLOC;

    return node;
}

static void
xenaml_write_node_internal(struct xenaml_node *node,
                           uint8_t **buffer_out)
{
    struct xenaml_node *child, *temp;

    if ( node == NULL )
        return;

    memcpy((*buffer_out), node->buffer, node->length);
    (*buffer_out) += node->length;

    child = node->children;
    while ( child != NULL )
    {
        temp = child;
        child = child->next;
        xenaml_write_node_internal(temp, buffer_out);
    }
}

void
xenaml_write_node(struct xenaml_node *node,
                  uint8_t **buffer_out)
{
    /* In case the top level node is the beginning of a list and not
     * a child, have to run over that first list.
     */
    while ( node != NULL )
    {
        xenaml_write_node_internal(node, buffer_out);
        node = node->next;
    }
}

static void
xenaml_calculate_length_internal(struct xenaml_node *node,
                                 uint32_t *length_out)
{
    struct xenaml_node *child, *temp;

    (*length_out) += node->length;

    child = node->children;
    while ( child != NULL )
    {
        temp = child;
        child = child->next;
        xenaml_calculate_length_internal(temp, length_out);
    }
}

void
xenaml_calculate_length(struct xenaml_node *node,
                        uint32_t *length_out)
{
    /* In case the top level node is the beginning of a list and not
     * a child, have to run over that first list.
     */
    while ( node != NULL )
    {
        xenaml_calculate_length_internal(node, length_out);
        node = node->next;
    }
}

void
xenaml_write_byte(uint8_t **buffer, uint8_t value)
{
    (*buffer)[0] = value;
    (*buffer)++;
}

void
xenaml_write_word(uint8_t **buffer, uint16_t value)
{
    (*buffer)[0] = value & 0xFF;
    (*buffer)[1] = (value >> 8) & 0xFF;
    (*buffer) += 2;
}

void
xenaml_write_dword(uint8_t **buffer, uint32_t value)
{
    (*buffer)[0] = value & 0xFF;
    (*buffer)[1] = (value >> 8) & 0xFF;
    (*buffer)[2] = (value >> 16) & 0xFF;
    (*buffer)[3] = (value >> 24) & 0xFF;
    (*buffer) += 4;
}

void
xenaml_write_qword(uint8_t **buffer, uint64_t value)
{
    (*buffer)[0] = value & 0xFF;
    (*buffer)[1] = (value >> 8) & 0xFF;
    (*buffer)[2] = (value >> 16) & 0xFF;
    (*buffer)[3] = (value >> 24) & 0xFF;
    (*buffer)[4] = (value >> 32) & 0xFF;
    (*buffer)[5] = (value >> 40) & 0xFF;
    (*buffer)[6] = (value >> 48) & 0xFF;
    (*buffer)[7] = (value >> 56) & 0xFF;
    (*buffer) += 8;
}

uint32_t
xenaml_package_length(uint8_t *pkg_len_buf, uint32_t pkg_len)
{
    /* Note that the package size must included the package length bytes also, so they
     * must be accounted for when determining which size to use. Crazy algorithm..
     */

    if ( (pkg_len + 1) < XENAML_PACKAGE_LEN_LIMIT1 )
    {
        pkg_len += 1;
        pkg_len_buf[0] = pkg_len; /* simple 1 byte length in 6 bits */

        return 1;
    }

    if ( (pkg_len + 2) < XENAML_PACKAGE_LEN_LIMIT2 )
    {
        pkg_len += 2;
        pkg_len_buf[0] = 0x40 | (pkg_len & 0xF); /* 0x40 is type, 0x0X is first nibble of length */
        pkg_len_buf[1] = (pkg_len >> 4) & 0xFF;  /* +1 bytes for rest length */

        return 2;
    }

    if ( (pkg_len + 3) < XENAML_PACKAGE_LEN_LIMIT3 )
    {
        pkg_len += 3;
        pkg_len_buf[0] = 0x80 | (pkg_len & 0xF); /* 0x80 is type, 0x0X is first nibble of length */
        pkg_len_buf[1] = (pkg_len >> 4) & 0xFF;  /* +2 bytes for rest length */
        pkg_len_buf[2] = (pkg_len >> 12) & 0xFF;

        return 3;
    }

    if ( (pkg_len + 4) < XENAML_PACKAGE_LEN_LIMIT4 )
    {
        pkg_len += 4;
        pkg_len_buf[0] = 0xC0 | (pkg_len & 0xF); /* 0xC0 is type, 0x0X is first nibble of length */
        pkg_len_buf[1] = (pkg_len >> 4) & 0xFF;  /* +3 bytes for rest of length */
        pkg_len_buf[2] = (pkg_len >> 12) & 0xFF;
        pkg_len_buf[3] = (pkg_len >> 20) & 0xFF;

        return 4;
    }

    return 0; /* Can't encode betond 2^28 bits, sorry */
}

EXTERNAL void
xenaml_delete_node(void *delete_node)
{
    struct xenaml_node *node = delete_node;
    struct xenaml_node *child, *temp;

    if ( node == NULL )
        return;

    if ( node->flags & XENAML_FLAG_PREMEM_ALLOC )
    {
        /* Can't do this */
        assert((node->flags & XENAML_FLAG_PREMEM_ALLOC) == 0);
        return;
    }

    child = node->children;
    while ( child != NULL )
    {
        temp = child;
        child = child->next;
        xenaml_reset_node(temp);
        xenaml_delete_node(temp);
    }

    if ( node->prev != NULL )
        node->prev->next = node->next;

    xenaml_reset_node(node);
    free(node);
}

EXTERNAL void
xenaml_delete_list(void *delete_node)
{
    struct xenaml_node *node = delete_node;
    struct xenaml_node *temp;

    if ( node == NULL )
        return;

    if ( node->flags & XENAML_FLAG_PREMEM_ALLOC )
    {
        /* Can't do this */
        assert((node->flags & XENAML_FLAG_PREMEM_ALLOC) == 0);
        return;
    }

    if ( node->prev != NULL )
        node->prev->next = NULL;

    while ( node != NULL )
    {
        temp = node;
        node = node->next;
        xenaml_reset_node(temp);
        xenaml_delete_node(temp);
    }
}

EXTERNAL
void* xenaml_next(void *current_node)
{
    if ( current_node == NULL )
        return NULL;

    return ((struct xenaml_node*)current_node)->next;
}

EXTERNAL void*
xenaml_children(void *current_node)
{
    if ( current_node == NULL )
        return NULL;

    return ((struct xenaml_node*)current_node)->children;
}

EXTERNAL int
xenaml_chain_children(void *current_node,
                      void *add_node,
                      int *error_out)
{
    struct xenaml_node *cnode = current_node;
    struct xenaml_node *anode = add_node;

    if ( cnode == NULL || anode == NULL )
        return xenacpi_error(error_out, EINVAL);

    /* Do not chain nodes together unless they are in an acceptable state */
    if ( anode->prev != NULL )
        return xenacpi_error(error_out, EPERM);
    else if ( cnode->children != NULL )
        return xenacpi_error(error_out, EPERM);

    cnode->children = anode;
    anode->prev = cnode;

    return 0;
}

EXTERNAL int
xenaml_chain_peers(void *current_node,
                   void *add_node,
                   int *error_out)
{
    struct xenaml_node *cnode = current_node;
    struct xenaml_node *anode = add_node;

    if ( cnode == NULL || anode == NULL )
        return xenacpi_error(error_out, EINVAL);

    /* Do not chain nodes together unless they are in an acceptable state */
    if ( anode->prev != NULL )
        return xenacpi_error(error_out, EPERM);
    if ( cnode->next != NULL )
        return xenacpi_error(error_out, EPERM);

    cnode->next = anode;
    anode->prev = cnode;

    return 0;
}

EXTERNAL int
xenaml_unchain_node(void *remove_node,
                    int *error_out)
{
    struct xenaml_node *rnode = remove_node;

    if ( rnode == NULL )
        return xenacpi_error(error_out, EINVAL);

    /* Do not unchain nodes unless they are in an acceptable state */
    if ( rnode->prev == NULL )
        return xenacpi_error(error_out, EPERM);

    if ( rnode->prev->next == rnode )
    {
        rnode->prev->next = rnode->next;
        xenaml_reset_node(rnode);
    }
    else if ( rnode->prev->children == rnode )
    {
        rnode->prev->children = rnode->next;
        xenaml_reset_node(rnode);
    }
    else
    {
        /* SNO your tree is hosed! */
        return xenacpi_error(error_out, EFAULT);
    }

    return 0;
}

EXTERNAL int
xenaml_create_ssdt(const char *oem_id,
                   const char *table_id,
                   uint32_t oem_rev,
                   void *pma,
                   void **root_out,
                   int *error_out)
{
    size_t oem_id_len, table_id_len;
    struct xenaml_node *root;
    uint8_t *ptr;

    if ( oem_id == NULL || table_id == NULL || root_out == NULL )
        return xenacpi_error(error_out, EINVAL);

    *root_out = NULL;

    oem_id_len = strlen(oem_id);
    table_id_len = strlen(table_id);

    if ( oem_id_len < 1 || oem_id_len > ACPI_OEM_ID_SIZE ||
         table_id_len < 1 || table_id_len > ACPI_OEM_TABLE_ID_SIZE )
        return xenacpi_error(error_out, EINVAL);

    root = xenaml_alloc_node(pma, sizeof(struct acpi_table_header), 1);
    if ( root == NULL )
        return xenacpi_error(error_out, ENOMEM);
    root->op = XENAML_UNASSIGNED_OPCODE;
    root->flags = XENAML_FLAG_DEFINITION_BLOCK;

    /* Fill in the SSDT header, note the length and checksum will be computed later */
    ptr = root->buffer;
    memcpy(ptr, ACPI_SIG_SSDT, ACPI_NAME_SIZE);
    ptr += ACPI_NAME_SIZE + sizeof(uint32_t);
    xenaml_write_byte(&ptr, XENAML_REVISION);
    ptr += 1; /* skip checksum - leave it zero */
    memcpy(ptr, oem_id, oem_id_len);
    ptr += ACPI_OEM_ID_SIZE;
    memcpy(ptr, table_id, table_id_len);
    ptr += ACPI_OEM_TABLE_ID_SIZE;
    xenaml_write_dword(&ptr, oem_rev);
    memcpy(ptr, XENAML_ASL_COMPILER_ID, ACPI_NAME_SIZE);
    ptr += ACPI_NAME_SIZE;
    xenaml_write_dword(&ptr, XENAML_ASL_COMPILER_REV);

    *root_out = root;

    return 0;
}

EXTERNAL int
xenaml_write_ssdt(void *root,
                  uint8_t **buffer_out,
                  uint32_t *length_out,
                  int *error_out)
{
    struct xenaml_node *ssdt = root;
    uint32_t total = 0, i;
    uint8_t *ptr;
    uint8_t sum;

    if ( ssdt == NULL || ssdt->children == NULL ||
         length_out == NULL || buffer_out == NULL )
        return xenacpi_error(error_out, EINVAL);

    xenaml_calculate_length(ssdt, &total);
    *buffer_out = malloc(total);
    if ( *buffer_out == NULL )
        return xenacpi_error(error_out, ENOMEM);
    *length_out = total;

    ptr = *buffer_out;
    xenaml_write_node(ssdt, &ptr);

    /* Write the length of the entire SSDT */
    ptr = *buffer_out;
    ptr += ACPI_NAME_SIZE;
    xenaml_write_dword(&ptr, total);

    /* Calculate the table checksum */
    ptr = *buffer_out;
    for (i = 0, sum = 0; i < total; i++)
        sum += ptr[i];

    ptr[XENAML_TABLE_CS_OFFSET] = -sum;

    return 0;
}

EXTERNAL void*
xenaml_create_premem(uint32_t size)
{
    struct xenaml_premem *premem;

    if ( sc_pagesize == 0 )
        sc_pagesize = (uint32_t)sysconf(_SC_PAGESIZE);

    if ( (size < sc_pagesize)||((size % sc_pagesize) != 0) )
        size = XENAML_ROUNDUP(size, sc_pagesize);

    premem = malloc(size);
    if ( premem == NULL )
        return NULL;

    premem->size = premem->free = size;
    premem->next = (uint8_t*)XENAML_MASK_ALIGN(((uint8_t*)premem + sizeof(struct xenaml_premem)));
    premem->free -= (uint32_t)(premem->next - (uint8_t*)premem);

    return premem;
}

EXTERNAL void
xenaml_free_premem(void *pma)
{
    free(pma);
}

