// SPDX-License-Identifier: GPL-2.0
#ifndef TSC_H
#define TSC_H
/**
 * \file
 *
 * Provides access to the CPU timestamp counter.
 *
 *//*
 * Copyright (C) 2020-2022 Martin Whitaker.
 */

#include "common.h"

#define rdtsc(low, high) assert(0)
#define rdtscl(low) assert(0)


/**
 * Reads and returns the timestamp counter value.
 */
static inline uint64_t get_tsc(void)
{
    return io_read(AM_TIMER_UPTIME).us;
}

#endif // TSC_H
