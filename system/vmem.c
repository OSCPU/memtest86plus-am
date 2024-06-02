// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2020-2022 Martin Whitaker.
//
// Derived from memtest86+ vmem.c
//
// vmem.c - MemTest-86
//
// Virtual memory handling (PAE)
//
// Released under version 2 of the Gnu Public License.
// By Chris Brady

#include <stdbool.h>
#include <stdint.h>

#include "vmem.h"

//------------------------------------------------------------------------------
// Private Variables
//------------------------------------------------------------------------------

static uintptr_t    mapped_window = 2;

//------------------------------------------------------------------------------
// Public Functions
//------------------------------------------------------------------------------

bool map_window(uintptr_t start_page)
{
    uintptr_t window = start_page >> (30 - PAGE_SHIFT);

    if (window < 2) {
        // Less than 2 GB so no mapping is required.
        return true;
    }

    mapped_window = window;
    return true;
}

void *first_word_mapping(uintptr_t page)
{
    void *result;
    if (page < PAGE_C(2,GB)) {
        // If the address is less than 2GB, it is directly mapped.
        result = (void *)(page << PAGE_SHIFT);
    } else {
        // Otherwise it is mapped to the third GB.
        uintptr_t alias = PAGE_C(2,GB) + page % PAGE_C(1,GB);
        result = (void *)(alias << PAGE_SHIFT);
    }
    return result;
}

void *last_word_mapping(uintptr_t page, size_t word_size)
{
    return (uint8_t *)first_word_mapping(page) + (PAGE_SIZE - word_size);
}

uintptr_t page_of(void *addr)
{
    uintptr_t page = (uintptr_t)addr >> PAGE_SHIFT;
    if (page >= PAGE_C(2,GB)) {
        page = page % PAGE_C(1,GB);
        page += mapped_window << (30 - PAGE_SHIFT);
    }
    return page;
}
