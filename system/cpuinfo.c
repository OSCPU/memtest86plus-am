// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2020-2022 Martin Whitaker.
// Copyright (C) 2004-2023 Sam Demeulemeester
//
// Derived from an extract of memtest86+ init.c:
//
// MemTest86+ V5 Specific code (GPL V2.0)
// ------------------------------------------------
// init.c - MemTest-86  Version 3.6
//
// Released under version 2 of the Gnu Public License.
// By Chris Brady

#include "common.h"

#include "config.h"

//------------------------------------------------------------------------------
// Public Variables
//------------------------------------------------------------------------------

int         l1_cache = 32;
int         l2_cache = 256;
int         l3_cache = 4096;

uint32_t    l1_cache_speed  = 0;
uint32_t    l2_cache_speed  = 0;
uint32_t    l3_cache_speed  = 0;
uint32_t    ram_speed = 0;

uint32_t    clks_per_msec = 0;

//------------------------------------------------------------------------------
// Private Functions
//------------------------------------------------------------------------------

static uint32_t memspeed(uintptr_t src, uint32_t len, int iter)
{
    uintptr_t dst;
    uintptr_t wlen;
    uint64_t start_time, end_time, run_time, overhead;
    int i;

    dst = src + len;

    wlen = len / sizeof(uintptr_t);
    start_time = io_read(AM_TIMER_UPTIME).us;
    for (i = 0; i < iter; i++) {
      volatile uintptr_t *pdst = (uintptr_t *)dst;
      volatile uintptr_t *psrc = (uintptr_t *)src;
      for (int j = 0; j < 0; j ++) {
        pdst[j] = psrc[j];
      }
    }
    end_time = io_read(AM_TIMER_UPTIME).us;

    overhead = (end_time - start_time);

    // Prime the cache
    uintptr_t *pdst = (uintptr_t *)dst;
    uintptr_t *psrc = (uintptr_t *)src;
    for (int j = 0; j < wlen; j ++) {
      pdst[j] = psrc[j];
    }

    // Write these bytes
    start_time = io_read(AM_TIMER_UPTIME).us;
    for (i = 0; i < iter; i++) {
      uintptr_t *pdst = (uintptr_t *)dst;
      uintptr_t *psrc = (uintptr_t *)src;
      for (int j = 0; j < wlen; j ++) {
        pdst[j] = psrc[j];
      }
    }
    end_time = io_read(AM_TIMER_UPTIME).us;

    if ((end_time - start_time) > overhead) {
        run_time = (end_time - start_time) - overhead;
    } else {
        return 0;
    }

    run_time = len * iter  * 1000 * 2 / run_time;

    return run_time;
}

static void measure_memory_bandwidth(void)
{
    uintptr_t bench_start_adr = (uintptr_t)heap.start;
    size_t mem_test_len;

    if (l3_cache) {
        mem_test_len = 4*l3_cache*1024;
    } else if (l2_cache) {
        mem_test_len = 4*l2_cache*1024;
    } else {
        return; // If we're not able to detect L2, don't start benchmark
    }


    // Measure L1 BW using 1/3rd of the total L1 cache size
    if (l1_cache) {
        l1_cache_speed = memspeed(bench_start_adr, (l1_cache/3)*1024, 50);
    }

    // Measure L2 BW using half the L2 cache size
    if (l2_cache) {
        l2_cache_speed = memspeed(bench_start_adr, l2_cache/2*1024, 50);
    }

    // Measure L3 BW using half the L3 cache size
    if (l3_cache) {
        l3_cache_speed = memspeed(bench_start_adr, l3_cache/2*1024, 50);
    }

    // Measure RAM BW
    ram_speed = memspeed(bench_start_adr, mem_test_len, 25);
}

//------------------------------------------------------------------------------
// Public Functions
//------------------------------------------------------------------------------

void membw_init(void)
{
    if(enable_bench) {
        measure_memory_bandwidth();
    }
}
