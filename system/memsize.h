// SPDX-License-Identifier: GPL-2.0
#ifndef MEMSIZE_H
#define MEMSIZE_H
/**
 * \file
 *
 * Provides some convenient constants and constant constructors.
 *
 *//*
 * Copyright (C) 2020-2022 Martin Whitaker.
 */

#include <stddef.h>
#include <stdint.h>

#define KB      10
#define MB      20
#define GB      30
#define TB      40

#define PAGE_SHIFT      12
#define PAGE_SIZE       (1 << PAGE_SHIFT)

#define PAGE_C(size, units) \
    (units < PAGE_SHIFT ? (uintptr_t)(size) << (PAGE_SHIFT - units) : (uintptr_t)(size) << (units - PAGE_SHIFT))

#define SIZE_C(size, units) \
    ((size_t)(size) << units)

#endif // MEMSIZE_H
