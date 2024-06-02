// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2020-2024 Martin Whitaker.
//
// Derived from memtest86+ memsize.c
//
// memsize.c - MemTest-86  Version 3.3
//
// Released under version 2 of the Gnu Public License.
// By Chris Brady

#include "common.h"

#include "memsize.h"
#include "pmem.h"

//------------------------------------------------------------------------------
// Public Variables
//------------------------------------------------------------------------------

pm_map_t    pm_map[MAX_MEM_SEGMENTS];

int         pm_map_size = 0;
size_t      num_pm_pages = 0;

//------------------------------------------------------------------------------
// Private Functions
//------------------------------------------------------------------------------

static void init_pm_map()
{
    pm_map[0].start = (uintptr_t)heap.start >> PAGE_SHIFT;
    pm_map[0].end = (uintptr_t)heap.end >> PAGE_SHIFT;
    pm_map_size ++;
    num_pm_pages = pm_map[0].end - pm_map[0].start;
}

static void sort_pm_map(void)
{
    // Do an insertion sort on the pm_map. On an already sorted list this should be a O(n) algorithm.
    for (int i = 1; i < pm_map_size; i++) {
        pm_map_t candidate = pm_map[i];
        int j = i;
        while ((j > 0) && (pm_map[j-1].start > candidate.start)) {
            pm_map[j] = pm_map[j-1];
            j--;
        }
        pm_map[j] = candidate;
    }
}

//------------------------------------------------------------------------------
// Public Functions
//------------------------------------------------------------------------------

void pmem_init(void)
{
    init_pm_map();
    sort_pm_map();
}
