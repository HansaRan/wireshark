/** @file
 * http://www.tty1.net/pycrc/faq_en.html#code-ownership
 *
 * Wireshark - Network traffic analyzer
 * By Gerald Combs <gerald@wireshark.org>
 * Copyright 1998 Gerald Combs
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/**
 * \file crc16-plain.h
 * Functions and types for CRC checks.
 *
 * Generated on Wed Mar 18 14:12:15 2009,
 * by pycrc v0.7, http://www.tty1.net/pycrc/
 * using the configuration:
 *    Width        = 16
 *    Poly         = 0x8005
 *    XorIn        = 0x0000
 *    ReflectIn    = True
 *    XorOut       = 0x0000
 *    ReflectOut   = True
 *    Algorithm    = table-driven
 *    Direct       = True
 *
 * Modified 2009-03-16 not to include <stdint.h> as our Win32 environment
 * appears not to have it; we're using GLib types, instead.
 *****************************************************************************/
#ifndef __CRC____PLAIN_H__
#define __CRC____PLAIN_H__

#include <stddef.h>
#include <stdint.h>

#include "ws_symbol_export.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * The definition of the used algorithm.
 *****************************************************************************/
#define CRC_ALGO_TABLE_DRIVEN 1

/**
 * The type of the CRC values.
 *
 * This type must be big enough to contain at least 16 bits.
 *****************************************************************************/
typedef uint16_t crc16_plain_t;

/**
 * Reflect all bits of a \a data word of \a data_len bytes.
 *
 * \param data         The data word to be reflected.
 * \param data_len     The width of \a data expressed in number of bits.
 * \return     The reflected data.
 *****************************************************************************/
long crc16_plain_reflect(long data, size_t data_len);

/**
 * Calculate the initial crc value.
 *
 * \return     The initial crc value.
 *****************************************************************************/
static inline crc16_plain_t crc16_plain_init(void)
{
    return 0x0000;
}

/**
 * Update the crc value with new data.
 *
 * \param crc      The current crc value.
 * \param data     Pointer to a buffer of \a data_len bytes.
 * \param data_len Number of bytes in the \a data buffer.
 * \return         The updated crc value.
 *****************************************************************************/
WS_DLL_PUBLIC
crc16_plain_t crc16_plain_update(crc16_plain_t crc, const unsigned char *data, size_t data_len);

/**
 * Calculate the final crc value.
 *
 * \param crc  The current crc value.
 * \return     The final crc value.
 *****************************************************************************/
static inline crc16_plain_t crc16_plain_finalize(crc16_plain_t crc)
{
    return crc ^ 0x0000;
}

/* Generated on Tue Jul 24 09:08:46 2012,
 * by pycrc v0.7.10, http://www.tty1.net/pycrc/
 * using the configuration:
 *    Width        = 16
 *    Poly         = 0x8005
 *    XorIn        = 0x0000
 *    ReflectIn    = False
 *    XorOut       = 0x0000
 *    ReflectOut   = False
 *    Algorithm    = table-driven
 *
 * Calculate the crc-16 (x^16 + x^15 + x^2 + 1) value for data. Note that this
 * CRC is not equal to crc16_plain.
 *
 * \param data     Pointer to a buffer of \a data_len bytes.
 * \param data_len Number of bytes in the \a data buffer.
 * \return         The crc value.
 *****************************************************************************/
WS_DLL_PUBLIC
uint16_t crc16_8005_noreflect_noxor(const uint8_t *data, uint64_t data_len);


#ifdef __cplusplus
}           /* closing brace for extern "C" */
#endif

#endif      /* __CRC____PLAIN_H__ */
