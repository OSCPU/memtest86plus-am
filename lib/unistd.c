// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2020-2022 Martin Whitaker.

#include "common.h"

#include "cpuinfo.h"

//------------------------------------------------------------------------------
// Public Functions
//------------------------------------------------------------------------------

void usleep(unsigned int usec)
{
    uint64_t now = io_read(AM_TIMER_UPTIME).us;
    uint64_t end = now + usec;
    while (io_read(AM_TIMER_UPTIME).us < end);
}

void sleep(unsigned int sec)
{
    while (sec > 0) {
        usleep(1000000);
        sec--;
    }
}
