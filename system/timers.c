// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2020-2022 Martin Whitaker.
// Copyright (C) 2004-2022 Sam Demeulemeester.

#include "common.h"

//------------------------------------------------------------------------------
// Private Functions
//------------------------------------------------------------------------------

static void correct_tsc(void)
{
    uint64_t start_time, end_time, run_time;
    volatile int loops = 0;

    start_time = io_read(AM_TIMER_UPTIME).us;
    for (; loops <= 100000000; loops ++);
    end_time = io_read(AM_TIMER_UPTIME).us;
    run_time = end_time - start_time;

    // Make sure we have a credible result
    if (run_time >= 50000) {
      extern uint32_t clks_per_msec;
      clks_per_msec = run_time / 50;
      return;
    }
}

//------------------------------------------------------------------------------
// Public Functions
//------------------------------------------------------------------------------

void timers_init(void)
{
    correct_tsc();
}
