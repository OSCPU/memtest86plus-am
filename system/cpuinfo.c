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
#include "pmem.h"
#include "vmem.h"

//------------------------------------------------------------------------------
// Constants
//------------------------------------------------------------------------------

#define BENCH_MIN_START_ADR 0x1000000   // 16MB

//------------------------------------------------------------------------------
// Public Variables
//------------------------------------------------------------------------------

const char  *cpu_model = NULL;

int         l1_cache = 0;
int         l2_cache = 0;
int         l3_cache = 0;

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
  assert(0);
#if 0
    uintptr_t dst;
    uintptr_t wlen;
    uint64_t start_time, end_time, run_time_clk, overhead;
    int i;

    dst = src + len;

#ifdef __x86_64__
    wlen = len / 8;
    // Get number of clock cycles due to overhead
    start_time = get_tsc();
    for (i = 0; i < iter; i++) {
        __asm__ __volatile__ (
            "movq %0,%%rsi\n\t" \
            "movq %1,%%rdi\n\t" \
            "movq %2,%%rcx\n\t" \
            "cld\n\t" \
            "rep\n\t" \
            "movsq\n\t" \
            :: "g" (src), "g" (dst), "g" (0)
            : "rsi", "rdi", "rcx"
        );
    }
    end_time = get_tsc();

    overhead = (end_time - start_time);

    // Prime the cache
    __asm__ __volatile__ (
        "movq %0,%%rsi\n\t" \
        "movq %1,%%rdi\n\t" \
        "movq %2,%%rcx\n\t" \
        "cld\n\t" \
        "rep\n\t" \
        "movsq\n\t" \
        :: "g" (src), "g" (dst), "g" (wlen)
        : "rsi", "rdi", "rcx"
    );

    // Write these bytes
    start_time = get_tsc();
    for (i = 0; i < iter; i++) {
        __asm__ __volatile__ (
            "movq %0,%%rsi\n\t" \
            "movq %1,%%rdi\n\t" \
            "movq %2,%%rcx\n\t" \
            "cld\n\t" \
            "rep\n\t" \
            "movsq\n\t" \
            :: "g" (src), "g" (dst), "g" (wlen)
            : "rsi", "rdi", "rcx"
          );
    }
    end_time = get_tsc();
#else
    wlen = len / 4;
    // Get number of clock cycles due to overhead
    start_time = get_tsc();
    for (i = 0; i < iter; i++) {
        __asm__ __volatile__ (
            "movl %0,%%esi\n\t" \
            "movl %1,%%edi\n\t" \
            "movl %2,%%ecx\n\t" \
            "cld\n\t" \
            "rep\n\t" \
            "movsl\n\t" \
            :: "g" (src), "g" (dst), "g" (0)
            : "esi", "edi", "ecx"
        );
    }
    end_time = get_tsc();

    overhead = (end_time - start_time);

    // Prime the cache
    __asm__ __volatile__ (
        "movl %0,%%esi\n\t" \
        "movl %1,%%edi\n\t" \
        "movl %2,%%ecx\n\t" \
        "cld\n\t" \
        "rep\n\t" \
        "movsl\n\t" \
        :: "g" (src), "g" (dst), "g" (wlen)
        : "esi", "edi", "ecx"
    );

    // Write these bytes
    start_time = get_tsc();
    for (i = 0; i < iter; i++) {
          __asm__ __volatile__ (
            "movl %0,%%esi\n\t" \
            "movl %1,%%edi\n\t" \
            "movl %2,%%ecx\n\t" \
            "cld\n\t" \
            "rep\n\t" \
            "movsl\n\t" \
            :: "g" (src), "g" (dst), "g" (wlen)
            : "esi", "edi", "ecx"
          );
    }
    end_time = get_tsc();
#endif

    if ((end_time - start_time) > overhead) {
        run_time_clk = (end_time - start_time) - overhead;
    } else {
        return 0;
    }

    run_time_clk = ((len * iter) / (double)run_time_clk) * clks_per_msec * 2;

    return run_time_clk;
#endif
}

static void measure_memory_bandwidth(void)
{
    uintptr_t bench_start_adr = 0;
    size_t mem_test_len;

    if (l3_cache) {
        mem_test_len = 4*l3_cache*1024;
    } else if (l2_cache) {
        mem_test_len = 4*l2_cache*1024;
    } else {
        return; // If we're not able to detect L2, don't start benchmark
    }

    // Locate enough free space for tests. We require the space to be mapped into
    // our virtual address space, which limits us to the first 2GB.
    for (int i = 0; i < pm_map_size && pm_map[i].start < VM_PINNED_SIZE; i++) {
        uintptr_t try_start = pm_map[i].start << PAGE_SHIFT;
        uintptr_t try_end   = try_start + mem_test_len * 2;

        // No start address below BENCH_MIN_START_ADR
        if (try_start < BENCH_MIN_START_ADR) {
            if ((pm_map[i].end << PAGE_SHIFT) >= (BENCH_MIN_START_ADR + mem_test_len * 2)) {
                try_start = BENCH_MIN_START_ADR;
                try_end   = BENCH_MIN_START_ADR + mem_test_len * 2;
            } else {
                continue;
            }
        }

        // Avoid the memory region where the program is currently located.
        extern uint8_t _start, _end;
        if (try_start < (uintptr_t)_end && try_end > (uintptr_t)_start) {
            try_start = (uintptr_t)_end;
            try_end   = try_start + mem_test_len * 2;
        }

        uintptr_t end_limit = (pm_map[i].end < VM_PINNED_SIZE ? pm_map[i].end : VM_PINNED_SIZE) << PAGE_SHIFT;
        if (try_end <= end_limit) {
            bench_start_adr = try_start;
            break;
        }
    }

    if (bench_start_adr == 0) {
        return;
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
