// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2020-2022 Martin Whitaker.

#include "common.h"

#include "cpulocal.h"
#include "barrier.h"

//------------------------------------------------------------------------------
// Public Functions
//------------------------------------------------------------------------------

void barrier_init(barrier_t *barrier, int num_threads)
{
    barrier->flag_num = allocate_local_flag();
    assert(barrier->flag_num >= 0);

    barrier_reset(barrier, num_threads);
}

void barrier_reset(barrier_t *barrier, int num_threads)
{
    barrier->num_threads = num_threads;
    barrier->count       = num_threads;

    local_flag_t *waiting_flags = local_flags(barrier->flag_num);
    waiting_flags[0].flag = false;
}

void barrier_spin_wait(barrier_t *barrier)
{
    if (barrier == NULL || barrier->num_threads < 2) {
        return;
    }
    local_flag_t *waiting_flags = local_flags(barrier->flag_num);
    int my_cpu = cpu_current();
    waiting_flags[my_cpu].flag = true;
    assert(0);
#if 0
    if (__sync_sub_and_fetch(&barrier->count, 1) != 0) {
        volatile bool *i_am_blocked = &waiting_flags[my_cpu].flag;
        while (*i_am_blocked) {
            __builtin_ia32_pause();
        }
        return;
    }
#endif
    // Last one here, so reset the barrier and wake the others. No need to
    // check if a CPU core is actually waiting - just clear all the flags.
    barrier->count = barrier->num_threads;
    __sync_synchronize();
    waiting_flags[0].flag = false;
}

void barrier_halt_wait(barrier_t *barrier)
{
    if (barrier == NULL || barrier->num_threads < 2) {
        return;
    }
    local_flag_t *waiting_flags = local_flags(barrier->flag_num);
    int my_cpu = cpu_current();
    waiting_flags[my_cpu].flag = true;
    //
    // There is a small window of opportunity for the wakeup signal to arrive
    // between us decrementing the barrier count and halting. So code the
    // following in assembler, both to ensure the window of opportunity is as
    // small as possible, and also to allow us to detect and skip over the
    // halt in the interrupt handler.
    //
    // if (__sync_sub_and_fetch(&barrier->count, 1) != 0) {
    //     __asm__ __volatile__ ("hlt");
    //     return;
    // }
    //
    assert(0);
#if 0
    __asm__ goto ("\t"
        "lock decl %0 \n\t"
        "je 0f        \n\t"
        "hlt          \n\t"
        "jmp %l[end]  \n"
        "0:           \n"
        : /* no outputs */
        : "m" (barrier->count)
        : /* no clobbers */
        : end
    );
#endif
    // Last one here, so reset the barrier and wake the others.
    barrier->count = barrier->num_threads;
    __sync_synchronize();
    waiting_flags[my_cpu].flag = false;
    if (waiting_flags[0].flag) {
      waiting_flags[0].flag = false;
    }
//end:
    return;
}
