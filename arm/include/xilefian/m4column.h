/*
===============================================================================

 Copyright (C) 2023 Felix Jones
 For conditions of distribution and use, see copyright notice in LICENSE

===============================================================================
*/

#ifndef XILEFIAN_M4COLUMN_H
#define XILEFIAN_M4COLUMN_H
#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

/**
* Packs exactly 640 bytes from src into dest
 * @param dest Pointer to the first byte of a column on a mode 4 frame-buffer (240x160 resolution frame-buffer)
 * @param src Pointer to four linear arrays of 160 bytes (160x4 pixel data)
 * @param n Number of rows to pack (Must be non-zero, and multiple of 4)
 */
void xilefian_m4col_pack4(void* __restrict__ dest, const void* __restrict__ src, size_t n) __attribute__((nonnull(1, 2)));

/**
 * Unpacks exactly 640 bytes from src into dest
 * @param dest Pointer to four linear arrays of 160 bytes (160x4 pixel data)
 * @param src Pointer to the first byte of a column on a mode 4 frame-buffer (240x160 resolution frame-buffer)
 * @param n Number of rows to unpack (Must be non-zero, and multiple of 4)
 */
void xilefian_m4col_unpack4(void* __restrict__ dest, const void* __restrict__ src, size_t n) __attribute__((nonnull(1, 2)));

/**
 * Packs exactly 320 bytes from src into dest
 * @param dest Pointer to the first byte of a column on a mode 4 frame-buffer (240x160 resolution frame-buffer)
 * @param src Pointer to two linear arrays of 160 bytes (160x2 pixel data)
 * @param n Number of rows to pack (Must be non-zero, and multiple of 4)
 */
void xilefian_m4col_pack2(void* __restrict__ dest, const void* __restrict__ src, size_t n) __attribute__((nonnull(1, 2)));

/**
 * Unpacks exactly 320 bytes from src into dest
 * @param dest Pointer to two linear arrays of 160 bytes (160x2 pixel data)
 * @param src Pointer to the first byte of a column on a mode 4 frame-buffer (240x160 resolution frame-buffer)
 * @param n Number of rows to unpack (Must be non-zero, and multiple of 4)
 */
void xilefian_m4col_unpack2(void* __restrict__ dest, const void* __restrict__ src, size_t n) __attribute__((nonnull(1, 2)));

#ifdef __cplusplus
}
#endif
#endif /* define XILEFIAN_M4COLUMN_H */
