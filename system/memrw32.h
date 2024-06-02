// SPDX-License-Identifier: GPL-2.0
#ifndef MEMRW32_H
#define MEMRW32_H
/**
 * \file
 *
 * Provides some 32-bit memory access functions. These stop the compiler
 * optimizing accesses which need to be ordered and atomic. Mostly used
 * for accessing memory-mapped hardware registers.
 *
 *//*
 * Copyright (C) 2021-2022 Martin Whitaker.
 */

#include <stdint.h>

/**
 * Reads and returns the value stored in the 32-bit memory location pointed
 * to by ptr.
 */
static inline uint32_t read32(const volatile uint32_t *ptr)
{
  return *ptr;
}

/**
 * Writes val to the 32-bit memory location pointed to by ptr.
 */
static inline void write32(volatile uint32_t *ptr, uint32_t val)
{
  *ptr = val;
}

/**
 * Writes val to the 32-bit memory location pointed to by ptr. Reads it
 * back (and discards it) to ensure the write is complete.
 */
static inline void flush32(volatile uint32_t *ptr, uint32_t val)
{
  write32(ptr, val);
  read32(ptr);
}

#endif // MEMRW32_H
