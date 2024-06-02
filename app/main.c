// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2020-2022 Martin Whitaker.
//
// Derived from memtest86+ main.c:
//
// MemTest86+ V5 Specific code (GPL V2.0)
// By Samuel DEMEULEMEESTER, memtest@memtest.org
// https://www.memtest.org
// ------------------------------------------------
// main.c - MemTest-86  Version 3.5
//
// Released under version 2 of the Gnu Public License.
// By Chris Brady

#include "common.h"
#include "unistd.h"

#include "cache.h"
#include "cpuinfo.h"
#include "serial.h"
#include "vmem.h"
#include "badram.h"
#include "display.h"
#include "error.h"
#include "tests.h"

//------------------------------------------------------------------------------
// Constants
//------------------------------------------------------------------------------

#ifndef TRACE_BARRIERS
#define TRACE_BARRIERS      0
#endif

#define LOW_LOAD_LIMIT      SIZE_C(4,MB)  // must be a multiple of the page size

//------------------------------------------------------------------------------
// Private Variables
//------------------------------------------------------------------------------

// The following variables are written by the current "master" CPU, but may
// be read by all active CPUs.

static volatile int     init_state = 0;

static barrier_t b0, b1;
static barrier_t *start_barrier = &b0;
static spinlock_t lk0;

static bool             start_run  = false;
static bool             start_pass = false;
static bool             start_test = false;
static bool             rerun_test = false;

static bool             dummy_run  = false;

static uintptr_t        window_start = 0;
static uintptr_t        window_end   = 0;

static size_t           num_mapped_pages = 0;

static int              test_stage = 0;

//------------------------------------------------------------------------------
// Public Variables
//------------------------------------------------------------------------------

// These are exposed in test.h.

uint8_t     chunk_index[MAX_CPUS];

int         num_active_cpus = 0;
int         num_enabled_cpus = 1;

int         master_cpu = 0;

barrier_t   *run_barrier = &b1;

spinlock_t  *error_mutex = &lk0;

vm_map_t    vm_map[MAX_MEM_SEGMENTS];
int         vm_map_size = 0;
uint32_t    proximity_domains[MAX_CPUS];

int         pass_num = 0;
int         test_num = 0;

int         window_num = 0;

bool        restart = false;
bool        bail    = false;

uintptr_t   test_addr[MAX_CPUS];

uint8_t _stacks[AP_STACK_SIZE];

//------------------------------------------------------------------------------
// Private Functions
//------------------------------------------------------------------------------

#define SHORT_BARRIER \
    if (TRACE_BARRIERS) { \
        trace(my_cpu, "Start barrier wait at %s line %i", __FILE__, __LINE__); \
    } \
    if (power_save < POWER_SAVE_HIGH) { \
        barrier_spin_wait(start_barrier); \
    } else { \
        barrier_halt_wait(start_barrier); \
    }

#define LONG_BARRIER \
    if (TRACE_BARRIERS) { \
        trace(my_cpu, "Start barrier wait at %s line %i", __FILE__, __LINE__); \
    } \
    if (power_save > POWER_SAVE_OFF) { \
        barrier_halt_wait(start_barrier); \
    } else { \
        barrier_spin_wait(start_barrier); \
    }

static void global_init(void)
{
    screen_init();

    pmem_init();

    membw_init();

    badram_init();

    config_init();

    tty_init();

    // At this point we have started reserving physical pages in the memory
    // map for data structures that need to be permanently pinned in place.
    // This may overwrite any data structures passed to us by the BIOS and/or
    // boot loader, e.g. the boot parameters, boot command line, and ACPI
    // tables. So do not access those data structures after this point.

    display_init();

    error_init();

    initial_config();

    clear_message_area();

    num_enabled_cpus = 1;
    chunk_index[0] = num_enabled_cpus;
    display_cpu_topology();

    master_cpu = 0;

    if (enable_trace) {
        display_pinned_message(0, 0,"CPU Trace");
        display_pinned_message(1, 0,"--- ----------------------------------------------------------------------------");
        set_scroll_lock(true);
    } else if (enable_sm) {
        post_display_init();
    }

    for (int i = 0; i < pm_map_size; i++) {
        trace(0, "pm %0*x - %0*x", 2*sizeof(uintptr_t), pm_map[i].start, 2*sizeof(uintptr_t), pm_map[i].end);
    }

    barrier_init(start_barrier, 1);
    barrier_init(run_barrier,   1);

    spin_unlock(error_mutex);

    start_run = true;
    dummy_run = true;
    restart = false;
}

static void setup_vm_map(uintptr_t win_start, uintptr_t win_end)
{
    vm_map_size = 0;

    num_mapped_pages = 0;

    // Reduce the window to fit in the user-specified limits.
    if (win_start < pm_limit_lower) {
        win_start = pm_limit_lower;
    }
    if (win_end > pm_limit_upper) {
        win_end = pm_limit_upper;
    }
    if (win_start >= win_end) {
        return;
    }

    // Now initialise the virtual memory map with the intersection
    // of the window and the physical memory segments.
    for (int i = 0; i < pm_map_size; i++) {
        // These are page numbers.
        uintptr_t seg_start = pm_map[i].start;
        uintptr_t seg_end   = pm_map[i].end;
        if (seg_start <= win_start) {
            seg_start = win_start;
        }
        if (seg_end >= win_end) {
            seg_end = win_end;
        }
        if (seg_start < seg_end && seg_start < win_end && seg_end > win_start) {
            num_mapped_pages += seg_end - seg_start;
            vm_map[vm_map_size].pm_base_addr = seg_start;
            vm_map[vm_map_size].start        = first_word_mapping(seg_start);
            vm_map[vm_map_size].end          = last_word_mapping(seg_end - 1, sizeof(testword_t));
            vm_map_size++;
        }
    }
}

static void test_all_windows(int my_cpu)
{
    bool parallel_test = false;
    bool i_am_master = (my_cpu == master_cpu);
    bool i_am_active = i_am_master;
    if (!dummy_run) {
        if (cpu_mode == PAR && test_list[test_num].cpu_mode == PAR) {
            parallel_test = true;
            i_am_active = true;
        }
    }
    if (i_am_master) {
        num_active_cpus = 1;
        if (!dummy_run) {
            if (parallel_test) {
                num_active_cpus = num_enabled_cpus;
                if(display_mode == DISPLAY_MODE_NA) {
                    display_all_active();
                }
            } else {
                if (display_mode == 0) {
                    display_active_cpu(my_cpu);
                }
            }
        }
        barrier_reset(run_barrier, num_active_cpus);
    }

    int iterations = test_list[test_num].iterations;
    if (pass_num == 0) {
        // Reduce iterations for a faster first pass.
        iterations /= 3;
    }

    // Loop through all possible windows.
    do {
        LONG_BARRIER;
        if (bail) {
            break;
        }

        if (i_am_master) {
            if (window_num == 0 && test_list[test_num].stages > 1) {
                // A multi-stage test runs through all the windows at each stage.
                // Relocation may disrupt the test.
                window_num = 1;
            }
            if (window_num == 0 && pm_limit_lower >= LOW_LOAD_LIMIT) {
                // Avoid unnecessary relocation.
                window_num = 1;
            }
        }
        SHORT_BARRIER;

        if (i_am_master) {
            //trace(my_cpu, "start window %i", window_num);
            switch (window_num) {
              case 0:
                window_start = 0;
                window_end   = (LOW_LOAD_LIMIT >> PAGE_SHIFT);
                break;
              case 1:
                window_start = (LOW_LOAD_LIMIT >> PAGE_SHIFT);
                window_end   = VM_WINDOW_SIZE;
                break;
              default:
                window_start = window_end;
                window_end  += VM_WINDOW_SIZE;
            }
            setup_vm_map(window_start, window_end);
        }
        SHORT_BARRIER;

        if (!i_am_active) {
            continue;
        }

        if (num_mapped_pages == 0) {
            // No memory to test in this window.
            if (i_am_master) {
                window_num++;
            }
            continue;
        }

        if (dummy_run) {
            if (i_am_master) {
                ticks_per_test[pass_num][test_num] += run_test(-1, test_num, test_stage, iterations);
            }
        } else {
            if (!map_window(vm_map[0].pm_base_addr)) {
                // Either there is no PAE or we are at the PAE limit.
                break;
            }
            run_test(my_cpu, test_num, test_stage, iterations);
        }

        if (i_am_master) {
            window_num++;
        }
    } while (window_end < pm_map[pm_map_size - 1].end);
}

static void select_next_master(void)
{
  master_cpu = 0;
}

//------------------------------------------------------------------------------
// Public Functions
//------------------------------------------------------------------------------

// The main entry point called from the startup code.

static Context *simple_trap(Event ev, Context *ctx) {
  switch (ev.event) {
    case EVENT_ERROR:
//      printf("ERROR@pc = %p: %s\n", ctx->eip, ev.msg);
      assert(0);
    default:;
  }
  return ctx;
}

void main(void)
{
  ioe_init();
  cte_init(simple_trap);
    int my_cpu;
    if (init_state == 0) {
        // If this is the first time here, we must be CPU 0, as the APs haven't been started yet.
        my_cpu = 0;
    } else {
        my_cpu = cpu_current();
    }
    if (init_state < 2) {
        cache_on();
        if (my_cpu == 0) {
            global_init();
            init_state = 1;
            if (enable_trace && num_enabled_cpus > 1) {
                set_scroll_lock(false);
                trace(0, "starting other CPUs");
            }
            barrier_reset(start_barrier, num_enabled_cpus);
            init_state = 2;
        } else {
            trace(my_cpu, "AP started");
            while (init_state < 2) {
                usleep(100);
            }
        }
    }

    // Due to the need to relocate ourselves in the middle of tests, the following
    // code cannot be written in the natural way as a set of nested loops. So we
    // have a single loop and use global state variables to allow us to restart
    // where we left off after each relocation.

    while (1) {
        SHORT_BARRIER;
        if (my_cpu == 0) {
            if (start_run) {
                pass_num = 0;
                start_pass = true;
                if (!dummy_run) {
                    display_start_run();
                    badram_init();
                    error_init();
                }
            }
            if (start_pass) {
                test_num = 0;
                start_test = true;
                if (dummy_run) {
                    ticks_per_pass[pass_num] = 0;
                } else {
                    display_start_pass();
                }
            }
            if (start_test) {
                trace(my_cpu, "start test %i", test_num);
                test_stage = 0;
                rerun_test = true;
                if (dummy_run) {
                    ticks_per_test[pass_num][test_num] = 0;
                } else if (test_list[test_num].enabled) {
                    display_start_test();
                }
                bail = false;
            }
            if (rerun_test) {
                window_num   = 0;
                window_start = 0;
                window_end   = 0;
            }
            start_run  = false;
            start_pass = false;
            start_test = false;
            rerun_test = false;
        }
        SHORT_BARRIER;
        if (test_list[test_num].enabled) {
            test_all_windows(my_cpu);
        }
        SHORT_BARRIER;
        if (my_cpu != 0) {
            continue;
        }

        check_input();
        if (restart) {
            // The configuration has been changed.
            master_cpu = 0;
            start_run = true;
            dummy_run = true;
            restart = false;
            continue;
        }
        error_update();

        if (test_list[test_num].enabled) {
            if (++test_stage < test_list[test_num].stages) {
                rerun_test = true;
                continue;
            }
            test_stage = 0;

            switch (cpu_mode) {
              case PAR:
                if (test_list[test_num].cpu_mode == SEQ) {
                    select_next_master();
                    if (master_cpu != 0) {
                        rerun_test = true;
                        continue;
                    }
                }
                break;
              case ONE:
                select_next_master();
                break;
              case SEQ:
                select_next_master();
                if (master_cpu != 0) {
                    rerun_test = true;
                    continue;
                }
                break;
              default:
                break;
            }
        }

        if (dummy_run) {
            ticks_per_pass[pass_num] += ticks_per_test[pass_num][test_num];
        }

        start_test = true;
        test_num++;
        if (test_num < NUM_TEST_PATTERNS) {
            continue;
        }

        pass_num++;
        if (dummy_run && pass_num == NUM_PASS_TYPES) {
            start_run = true;
            dummy_run = false;
            continue;
        }

        start_pass = true;
        if (!dummy_run) {
            display_pass_count(pass_num);
            if (error_count == 0) {
                display_status("Pass   ");
                display_big_status(true);
            } else {
                display_big_status(false);
            }
        }
    }
}
