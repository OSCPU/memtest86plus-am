// SPDX-License-Identifier: GPL-2.0
#ifndef CPUINFO_H
#define CPUINFO_H
/**
 * \file
 *
 * Provides information about the CPU type, clock speed and cache sizes.
 *
 *//*
 * Copyright (C) 2020-2022 Martin Whitaker.
 * Copyright (C) 2004-2023 Sam Demeulemeester.
 */

/**
 * The size of the L1 cache in KB.
 */
extern int l1_cache;

/**
 * The size of the L2 cache in KB.
 */
extern int l2_cache;

/**
 * The size of the L3 cache in KB.
 */
extern int l3_cache;

/**
 * The bandwidth of the L1 cache
 */
extern uint32_t l1_cache_speed;

/**
 * The bandwidth of the L2 cache
 */
extern uint32_t l2_cache_speed;

/**
 * The bandwidth of the L3 cache
 */
extern uint32_t l3_cache_speed;

/**
 * The bandwidth of the RAM
 */
extern uint32_t ram_speed;

/**
 * The TSC clock speed in kHz. Assumed to be the nominal CPU clock speed.
 */
extern uint32_t clks_per_msec;

/**
 * Determines the CPU info and stores it in the exported variables.
 */
void cpuinfo_init(void);

/**
 * Determines the RAM & caches bandwidth and stores it in the exported variables.
 */
void membw_init(void);

#endif // CPUINFO_H
