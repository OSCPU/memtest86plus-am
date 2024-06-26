// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2020-2022 Martin Whitaker.
// Copyright (C) 2004-2022 Sam Demeulemeester.
//
// Derived from memtest86+ config.c:
//
// ----------------------------------------------------
// config.c - MemTest-86  Version 3.4
//
// Released under version 2 of the Gnu Public License.
// By Chris Brady

#include "common.h"

#include "serial.h"
#include "vmem.h"
#include "read.h"
#include "unistd.h"
#include "display.h"
#include "tests.h"

//------------------------------------------------------------------------------
// Constants
//------------------------------------------------------------------------------

// Origin and size of the pop-up window.

#define POP_R       3
#define POP_C       21

#define POP_W       38
#define POP_H       18

#define POP_LAST_R  (POP_R + POP_H - 1)
#define POP_LAST_C  (POP_C + POP_W - 1)

#define POP_REGION  POP_R, POP_C, POP_LAST_R, POP_LAST_C

#define POP_LM      (POP_C + 3)     // Left margin
#define POP_LI      (POP_C + 5)     // List indent

#define SEL_W       32
#define SEL_H       2

#define SEL_AREA    (SEL_W * SEL_H)

//------------------------------------------------------------------------------
// Private Variables
//------------------------------------------------------------------------------

static uint16_t popup_save_buffer[POP_W * POP_H];

//------------------------------------------------------------------------------
// Public Variables
//------------------------------------------------------------------------------

uintptr_t       pm_limit_lower = 0;
uintptr_t       pm_limit_upper = 0;

uintptr_t       num_pages_to_test = 0;

cpu_mode_t      cpu_mode = PAR;

error_mode_t    error_mode = ERROR_MODE_NONE;

bool            exclude_ecores     = true;

bool            smp_enabled        = false;

bool            enable_big_status  = true;
bool            enable_temperature = true;
bool            enable_trace       = true;

bool            enable_sm          = true;
bool            enable_bench       = true;
bool            enable_mch_read    = true;
bool            enable_numa        = false;

bool            enable_ecc_polling = false;

bool            pause_at_start     = false;

power_save_t    power_save         = POWER_SAVE_HIGH;

bool            enable_tty         = false;
uintptr_t       tty_address        = 0x3F8;             // Legacy IO or MMIO Address accepted
int             tty_baud_rate      = 115200;
int             tty_update_period  = 2;                 // Update TTY every 2 seconds (default)

uint32_t        tty_mmio_ref_clk   = UART_REF_CLK_MMIO; // Reference clock for MMIO (in Hz)
int             tty_mmio_stride    = 4;                 // Stride for MMIO (register width in bytes)

bool            err_banner_redraw  = false;             // Redraw banner on new errors

//------------------------------------------------------------------------------
// Private Functions
//------------------------------------------------------------------------------

static void parse_serial_params(const char *params)
{
    enable_tty = true;

    // No parameters passed (only "console"), use default
     if (params == NULL) {
        return;
    }

    // Check if console is MMIO and grab address and stride
    uintptr_t mmio_adr = 0;
    if (strncmp(params, "mmio,0x", 7) == 0) {
        mmio_adr = hexstr2int(params+7);
        tty_mmio_stride = 1;
    } else if (strncmp(params, "mmio16,0x", 9) == 0) {
        mmio_adr = hexstr2int(params+9);
        tty_mmio_stride = 2;
    } else if (strncmp(params, "mmio32,0x", 9) == 0) {
        mmio_adr = hexstr2int(params+9);
        tty_mmio_stride = 4;
    }

    if(strncmp(params, "mmio", 4) == 0) {
        if (mmio_adr > 0xFFFF) {
            tty_address = mmio_adr;
        } else {
            enable_tty = false;
        }
        return;
    }

    // No TTY port passed, use default ttyS0
    if (strncmp(params, "ttyS", 5) == 0) {
        return;
    }

    // Configure TTY port or use default
    if (params[4] >= '0' && params[4] <= '3') {
        tty_address = serial_io_ports[params[4] - '0'];
    } else {
        return;
    }

    // No Baud Rate specified, use default
    if (params[5] != ',' || params[6] == '\0') {
        return;
    }

    switch (params[6])
    {
        default:
            return;
        case '1':
            tty_baud_rate   = (params[7] == '9') ? 19200 : 115200;
            tty_update_period = (params[7] == '9') ? 4 : 2;
            break;
        case '2':
            tty_baud_rate   = 230400;
            tty_update_period = 2;
            break;
        case '3':
            tty_baud_rate   = 38400;
            tty_update_period = 4;
            break;
        case '5':
            tty_baud_rate   = 57600;
            tty_update_period = 3;
            break;
        case '7':
            tty_baud_rate   = 76800;
            tty_update_period = 3;
            break;
        case '9':
            tty_baud_rate   = 9600;
            tty_update_period = 5;
            break;
    }

}

static void parse_option(const char *option, const char *params)
{
    if (option[0] == '\0') return;

    if (strncmp(option, "console", 8) == 0) {
        parse_serial_params(params);
    } else if (strncmp(option, "cpuseqmode", 11) == 0) {
        if (strncmp(params, "par", 4) == 0) {
            cpu_mode = PAR;
        } else if (strncmp(params, "seq", 4) == 0) {
            cpu_mode = SEQ;
        } else if (strncmp(params, "rr", 3) == 0 || strncmp(params, "one", 4) == 0) {
            cpu_mode = ONE;
        }
    } else if (strncmp(option, "reportmode", 11) == 0) {
        if (strncmp(params, "none", 5) == 0) {
            error_mode = ERROR_MODE_NONE;
        } else if (strncmp(params, "summary", 8) == 0) {
            error_mode = ERROR_MODE_SUMMARY;
        } else if (strncmp(params, "address", 8) == 0) {
            error_mode = ERROR_MODE_ADDRESS;
        } else if (strncmp(params, "badram", 7) == 0) {
            error_mode = ERROR_MODE_BADRAM;
        }
    } else if (strncmp(option, "nobench", 8) == 0) {
        enable_bench = false;
    } else if (strncmp(option, "nobigstatus", 12) == 0) {
        enable_big_status = false;
    } else if (strncmp(option, "nomch", 6) == 0) {
        enable_mch_read = false;
    } else if (strncmp(option, "nopause", 8) == 0) {
        pause_at_start = false;
    } else if (strncmp(option, "nosm", 5) == 0) {
        enable_sm = false;
    } else if (strncmp(option, "nosmp", 6) == 0) {
        smp_enabled = false;
    } else if (strncmp(option, "numa", 5) == 0) {
        enable_numa = true;
    } else if (strncmp(option, "nonuma", 7) == 0) {
        enable_numa = false;
    } else if (strncmp(option, "powersave", 10) == 0) {
        if (strncmp(params, "off", 4) == 0) {
            power_save = POWER_SAVE_OFF;
        } else if (strncmp(params, "low", 4) == 0) {
            power_save = POWER_SAVE_LOW;
        } else if (strncmp(params, "high", 5) == 0) {
            power_save = POWER_SAVE_HIGH;
        }
    } else if (strncmp(option, "trace", 6) == 0) {
        enable_trace = true;
    }
}

void parse_command_line(char *cmd_line, int cmd_line_size)
{
    const char *option = cmd_line;
    const char *params = NULL;
    for (int i = 0; i < cmd_line_size; i++) {
        switch (cmd_line[i]) {
          case '\0':
            parse_option(option, params);
            return;
          case ' ':
            cmd_line[i] = '\0';
            parse_option(option, params);
            option = &cmd_line[i+1];
            params = NULL;
            break;
          case '=':
            cmd_line[i] = '\0';
            params = &cmd_line[i+1];
            break;
          default:
            break;
        }
    }
}

static void update_num_pages_to_test(void)
{
    num_pages_to_test = 0;
    for (int i = 0; i < pm_map_size; i++) {
        if (pm_map[i].start >= pm_limit_lower && pm_map[i].end <= pm_limit_upper) {
            num_pages_to_test += pm_map[i].end - pm_map[i].start;
            continue;
        }
        if (pm_map[i].start < pm_limit_lower) {
            if (pm_map[i].end < pm_limit_lower) {
                continue;
            }
            if (pm_map[i].end > pm_limit_upper) {
                num_pages_to_test += pm_limit_upper - pm_limit_lower;
            } else {
                num_pages_to_test += pm_map[i].end - pm_limit_lower;
            }
            continue;
        }
        if (pm_map[i].end > pm_limit_upper) {
            if (pm_map[i].start > pm_limit_upper) {
                continue;
            }
            num_pages_to_test += pm_limit_upper - pm_map[i].start;
        }
    }
}

static void clear_popup_row(int row)
{
    clear_screen_region(row, POP_C, row, POP_LAST_C);
}

static void display_input_message(int row, const char *message)
{
    clear_popup_row(row);
    prints(row, POP_LM, message);
    if (enable_tty) tty_send_region(POP_REGION);
}

static void display_error_message(int row, const char *message)
{
    clear_popup_row(row);
    set_foreground_colour(YELLOW);
    prints(row, POP_LM, message);
    set_foreground_colour(WHITE);
    if (enable_tty) tty_send_region(POP_REGION);
}

static void display_selection_header(int row, int max_num, int offset)
{
    int i;

    prints(row, POP_LM, "Current selection:");
    if (max_num >= SEL_AREA) {
        prints(row, POP_LM+18, "  (scroll U D)");
        printc(row, POP_LM+28, 0x18);
        printc(row, POP_LM+30, 0x19);
    }
    row++;
    printi(row, POP_LM-2, offset, 3, false, false);
    offset++;
    for (i = 1; i < SEL_W && offset < max_num; i++) {
        printc(row, POP_LM+i, i%8 || (max_num < 16) ? 0xc4 : 0xc2);
        offset++;
    }
    if (i == SEL_W) {
        int data_rows = (max_num + SEL_W) / SEL_W;
        if (data_rows > SEL_H) {
            data_rows = SEL_H;
        }
        row += data_rows + 1;
        offset += SEL_W * (data_rows - 2);
        for (i = 0; i < (SEL_W - 1) && offset < max_num; i++) {
            printc(row, POP_LM+i, i==0 ? 0xc0 : i%8 ? 0xc4 : 0xc1);
            offset++;
        }
    }
    printi(row, POP_LM+i, offset, 3, false, true);
}

static void display_enabled(int row, int n, bool enabled)
{
    if (n >= 0 && n < SEL_AREA) {
        printc(row + (n / SEL_W), POP_LM + (n % SEL_W), enabled ? '*' : '.');
    }
}

static bool set_all_tests(bool enabled)
{
    clear_popup_row(POP_R+14);
    for (int i = 0; i < NUM_TEST_PATTERNS; i++) {
        test_list[i].enabled = enabled;
        display_enabled(POP_R+12, i, enabled);
    }
    return true;
}

static bool add_or_remove_test(bool add)
{

    display_input_message(POP_R+14, "Enter test #");
    int n = read_value(POP_R+14, POP_LM+12, 2, 0);
    if (n < 0 || n >= NUM_TEST_PATTERNS) {
        display_error_message(POP_R+14, "Invalid test number");
        return false;
    }
    test_list[n].enabled = add;
    display_enabled(POP_R+12, n, add);
    clear_popup_row(POP_R+14);
    return true;
}

static bool add_test_range()
{
    display_input_message(POP_R+14, "Enter first test #");
    int n1 = read_value(POP_R+14, POP_LM+18, 2, 0);
    if (n1 < 0 || n1 >= NUM_TEST_PATTERNS) {
        display_error_message(POP_R+14, "Invalid test number");
        return false;
    }
    display_input_message(POP_R+14, "Enter last test #");
    int n2 = read_value(POP_R+14, POP_LM+17, 2, 0);
    if (n2 < n1 || n2 >= NUM_TEST_PATTERNS) {
        display_error_message(POP_R+14, "Invalid test range");
        return false;
    }
    for (int i = n1; i <= n2; i++) {
        test_list[i].enabled = true;
        display_enabled(POP_R+12, i, true);
    }
    clear_popup_row(POP_R+14);
    return true;
}

static void test_selection_menu(void)
{
    clear_screen_region(POP_REGION);
    prints(POP_R+1, POP_LM, "Test Selection:");
    prints(POP_R+3, POP_LI, "<F1>  Clear selection");
    prints(POP_R+4, POP_LI, "<F2>  Remove one test");
    prints(POP_R+5, POP_LI, "<F3>  Add one test");
    prints(POP_R+6, POP_LI, "<F4>  Add test range");
    prints(POP_R+7, POP_LI, "<F5>  Add all tests");
    prints(POP_R+8, POP_LI, "<F10> Exit menu");

    display_selection_header(POP_R+10, NUM_TEST_PATTERNS - 1, 0);
    for (int i = 0; i < NUM_TEST_PATTERNS; i++) {
        display_enabled(POP_R+12, i, test_list[i].enabled);
    }

    bool tty_update = enable_tty;
    bool exit_menu = false;
    while (!exit_menu) {
        bool changed = false;

        if (tty_update) {
            tty_send_region(POP_REGION);
        }
        tty_update = enable_tty;

        switch (get_key()) {
          case '1':
            changed = set_all_tests(false);
            break;
          case '2':
            changed = add_or_remove_test(false);
            break;
          case '3':
            changed = add_or_remove_test(true);
            break;
          case '4':
            changed = add_test_range();
            break;
          case '5':
            changed = set_all_tests(true);
            break;
          case '0': {
            clear_popup_row(POP_R+14);
            int num_selected = 0;
            for (int i = 0; i < NUM_TEST_PATTERNS; i++) {
                if (test_list[i].enabled) {
                    num_selected++;
                }
            }
            if (num_selected > 0) {
                exit_menu = true;
            } else {
                display_error_message(POP_R+14, "You must select at least one test");
            }
          } break;
          default:
            usleep(1000);
            tty_update = false;
            break;
        }
        if (changed) {
            restart = true;
            changed = false;
        }
    }

    clear_screen_region(POP_REGION);
}

static void address_range_menu(void)
{
    clear_screen_region(POP_REGION);
    prints(POP_R+1, POP_LM, "Address Range:");
    prints(POP_R+3, POP_LI, "<F1>  Set lower limit");
    prints(POP_R+4, POP_LI, "<F2>  Set upper limit");
    prints(POP_R+5, POP_LI, "<F3>  Test all memory");
    prints(POP_R+6, POP_LI, "<F10> Exit menu");
    printk(POP_R+8, POP_LM, "Current range: %kB - %kB", pm_limit_lower << 2, pm_limit_upper << 2);

    bool tty_update = enable_tty;
    bool exit_menu = false;
    while (!exit_menu) {
        bool changed = false;

        if (tty_update) {
            tty_send_region(POP_REGION);
        }
        tty_update = enable_tty;

        switch (get_key()) {
          case '1': {
            display_input_message(POP_R+10, "Enter lower limit: ");
            uintptr_t page = read_value(POP_R+10, POP_LM+19, 15, -PAGE_SHIFT);
            if (page < pm_limit_upper) {
                clear_popup_row(POP_R+10);
                pm_limit_lower = page;
                changed = true;
            } else {
                display_error_message(POP_R+10, "Lower must be less than upper");
            }
          } break;
          case '2': {
            display_input_message(POP_R+10, "Enter upper limit: ");
            uintptr_t page = read_value(POP_R+10, POP_LM+19, 15, -PAGE_SHIFT);
            if (page > pm_limit_lower) {
                clear_popup_row(POP_R+10);
                pm_limit_upper = page;
                changed = true;
            } else {
                display_error_message(POP_R+10, "Upper must be greater than lower");
            }
          } break;
          case '3':
            clear_popup_row(POP_R+10);
            pm_limit_lower = 0;
            pm_limit_upper = pm_map[pm_map_size - 1].end;
            changed = true;
            break;
          case '0':
            exit_menu = true;
            break;
          default:
            usleep(1000);
            tty_update = false;
            break;
        }
        if (changed) {
            clear_popup_row(POP_R+8);
            printk(POP_R+8, POP_LM, "Current range: %kB - %kB", pm_limit_lower << 2, pm_limit_upper << 2);
            update_num_pages_to_test();
            restart = true;
            changed = false;
        }
    }

    clear_screen_region(POP_REGION);
}

static void set_cpu_mode(cpu_mode_t mode)
{
    printc(POP_R+3+cpu_mode, POP_LM, ' ');
    cpu_mode = mode;
    printc(POP_R+3+cpu_mode, POP_LM, '*');
}

static void cpu_mode_menu(void)
{
    clear_screen_region(POP_REGION);
    prints(POP_R+1, POP_LM, "CPU Sequencing Mode:");
    prints(POP_R+3, POP_LI, "<F1>  Parallel    (PAR)");
    prints(POP_R+4, POP_LI, "<F2>  Sequential  (SEQ)");
    prints(POP_R+5, POP_LI, "<F3>  Round robin (RR)");
    prints(POP_R+6, POP_LI, "<F10> Exit menu");
    printc(POP_R+3+cpu_mode, POP_LM, '*');

    bool tty_update = enable_tty;
    bool exit_menu = false;
    while (!exit_menu) {
        int ch = get_key();

        if (tty_update) {
            tty_send_region(POP_REGION);
        }
        tty_update = enable_tty;

        switch (ch) {
          case '1':
          case '2':
          case '3':
            set_cpu_mode(ch - '1');
            break;
          case 'u':
            if (cpu_mode > 0) {
                set_cpu_mode(cpu_mode - 1);
            }
            break;
          case 'd':
            if (cpu_mode < 2) {
                set_cpu_mode(cpu_mode + 1);
            }
            break;
          case '0':
            exit_menu = true;
            break;
          default:
            usleep(1000);
            tty_update = false;
            break;
        }
    }

    clear_screen_region(POP_REGION);
}

static void set_error_mode(error_mode_t mode)
{
    printc(POP_R+3+error_mode, POP_LM, ' ');
    error_mode = mode;
    printc(POP_R+3+error_mode, POP_LM, '*');
}

static void error_mode_menu(void)
{
    clear_screen_region(POP_REGION);
    prints(POP_R+1, POP_LM, "Error Reporting Mode:");
    prints(POP_R+3, POP_LI, "<F1>  Error counts only");
    prints(POP_R+4, POP_LI, "<F2>  Error summary");
    prints(POP_R+5, POP_LI, "<F3>  Individual errors");
    prints(POP_R+6, POP_LI, "<F4>  BadRAM patterns");
    prints(POP_R+7, POP_LI, "<F10> Exit menu");
    printc(POP_R+3+error_mode, POP_LM, '*');

    bool tty_update = enable_tty;
    bool exit_menu = false;
    while (!exit_menu) {
        int ch = get_key();

        if (tty_update) {
            tty_send_region(POP_REGION);
        }

        tty_update = enable_tty;

        switch (ch) {
          case '1':
          case '2':
          case '3':
          case '4':
            set_error_mode(ch - '1');
            break;
          case 'u':
            if (error_mode > 0) {
                set_error_mode(error_mode - 1);
            }
            break;
          case 'd':
            if (error_mode < 3) {
                set_error_mode(error_mode + 1);
            }
            break;
          case '0':
            exit_menu = true;
            break;
          default:
            usleep(1000);
            tty_update = false;
            break;
        }
    }

    clear_screen_region(POP_REGION);
}

//------------------------------------------------------------------------------
// Public Functions
//------------------------------------------------------------------------------

void config_init(void)
{
    pm_limit_lower = 0;
    pm_limit_upper = pm_map[pm_map_size - 1].end;

    update_num_pages_to_test();

    cpu_mode = PAR;

    error_mode = ERROR_MODE_ADDRESS;

    power_save = POWER_SAVE_HIGH;
}

void config_menu(bool initial)
{
    save_screen_region(POP_REGION, popup_save_buffer);
    set_background_colour(BLACK);
    clear_screen_region(POP_REGION);

    cpu_mode_t   old_cpu_mode   = cpu_mode;

    bool tty_update = enable_tty;
    bool exit_menu = false;
    while (!exit_menu) {
        prints(POP_R+1,  POP_LM, "Settings:");
        prints(POP_R+3,  POP_LI, "<F1>  Test selection");
        prints(POP_R+4,  POP_LI, "<F2>  Address range");
        prints(POP_R+5,  POP_LI, "<F3>  CPU sequencing mode");
        prints(POP_R+6,  POP_LI, "<F4>  Error reporting mode");
        if (initial) {
            //if (no_temperature) set_foreground_colour(BOLD+BLACK);
            printk(POP_R+8,  POP_LI, "<F6>  Temperature %s", enable_temperature ? "disable" : "enable ");
            //if (no_temperature) set_foreground_colour(WHITE);
            printk(POP_R+9,  POP_LI, "<F7>  Boot trace %s",  enable_trace  ? "disable" : "enable ");
            prints(POP_R+10, POP_LI, "<F10> Exit menu");
        } else {
            prints(POP_R+7,  POP_LI, "<F5>  Skip current test");
            prints(POP_R+8 , POP_LI, "<F10> Exit menu");
        }

        if (tty_update) {
            tty_send_region(POP_REGION);
        }

        tty_update = enable_tty;

        switch (get_key()) {
          case '1':
            test_selection_menu();
            break;
          case '2':
            address_range_menu();
            break;
          case '3':
            cpu_mode_menu();
            break;
          case '4':
            error_mode_menu();
            break;
          case '5':
            if (!initial) {
                exit_menu = true;
                bail = true;
            }
            break;
          case '6':
            if (initial) {
                enable_temperature = !enable_temperature;
            }
            break;
          case '7':
            if (initial) {
                enable_trace = !enable_trace;
            }
            break;
          case '0':
            exit_menu = true;
            break;
          default:
            usleep(1000);
            tty_update = false;
            break;
        }
    }

    restore_screen_region(POP_REGION, popup_save_buffer);
    set_background_colour(BLUE);

    if (enable_tty) {
        tty_send_region(POP_REGION);
    }

    if (cpu_mode != old_cpu_mode) {
        display_cpu_topology();
        restart = true;
    }

    if (restart) {
        bail = true;
    }
}

void initial_config(void)
{
    if (pause_at_start) {
        bool got_key = false;
        for (int i = 0; i < 3000 && !got_key; i++) {
            usleep(1000);
            switch (get_key()) {
              case ESC:
                clear_message_area();
                display_notice("Exiting...");
                halt(0);
                break;
              case '1':
                config_menu(true);
                got_key = true;
                break;
              case ' ':
                toggle_scroll_lock();
                break;
              case '\n':
                got_key = true;
                break;
              default:
                break;
            }
        }
    }
}
