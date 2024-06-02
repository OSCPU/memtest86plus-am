// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2020-2024 Martin Whitaker

#include "common.h"

#include "screen.h"

//------------------------------------------------------------------------------
// Private Variables
//------------------------------------------------------------------------------

vga_buffer_t shadow_buffer;

static uint8_t current_attr = WHITE | BLUE << 4;

//------------------------------------------------------------------------------
// Private Functions
//------------------------------------------------------------------------------

static void vga_put_char(int row, int col, uint8_t ch, uint8_t attr)
{
    shadow_buffer[row][col].ch   = ch;
    shadow_buffer[row][col].attr = attr;

    printf("\033[%d;%dH%c", row + 1, col + 1, ch);
}

static void (*put_char)(int, int, uint8_t, uint8_t) = vga_put_char;

static void put_value(int row, int col, uint16_t value)
{
    put_char(row, col, value % 256, value / 256);
}

//------------------------------------------------------------------------------
// Public Functions
//------------------------------------------------------------------------------

void screen_init(void)
{
}

void set_foreground_colour(screen_colour_t colour)
{
    current_attr = (current_attr & 0xf0) | (colour & 0x0f);
}

void set_background_colour(screen_colour_t  colour)
{
    current_attr = (current_attr & 0x8f) | ((colour << 4) & 0x70);
}

void clear_screen(void)
{
    for (int row = 0; row < SCREEN_HEIGHT; row++) {
        for (int col = 0; col < SCREEN_WIDTH; col++) {
            put_char(row, col, ' ', current_attr);
        }
    }
}

void clear_screen_region(int start_row, int start_col, int end_row, int end_col)
{
    if (start_row < 0) start_row = 0;
    if (start_col < 0) start_col = 0;

    if (end_row >= SCREEN_HEIGHT) end_row = SCREEN_HEIGHT - 1;
    if (end_col >= SCREEN_WIDTH)  end_col = SCREEN_WIDTH  - 1;

    if (start_row > end_row) return;
    if (start_col > end_col) return;

    for (int row = start_row; row <= end_row; row++) {
        for (int col = start_col; col <= end_col; col++) {
            put_char(row, col, ' ', current_attr);
        }
    }
}

void scroll_screen_region(int start_row, int start_col, int end_row, int end_col)
{
    if (start_row < 0) start_row = 0;
    if (start_col < 0) start_col = 0;

    if (end_row >= SCREEN_HEIGHT) end_row = SCREEN_HEIGHT - 1;
    if (end_col >= SCREEN_WIDTH)  end_col = SCREEN_WIDTH  - 1;

    if (start_row > end_row) return;
    if (start_col > end_col) return;

    for (int row = start_row; row <= end_row; row++) {
        for (int col = start_col; col <= end_col; col++) {
            if (row < end_row) {
                put_value(row, col, shadow_buffer[row + 1][col].value);
            } else {
                put_char(row, col, ' ', current_attr);
            }
        }
    }
}

void save_screen_region(int start_row, int start_col, int end_row, int end_col, uint16_t buffer[])
{
    if (start_row < 0) start_row = 0;
    if (start_col < 0) start_col = 0;

    uint16_t *dst = &buffer[0];
    for (int row = start_row; row <= end_row; row++) {
        if (row >= SCREEN_HEIGHT) break;
        for (int col = start_col; col <= end_col; col++) {
            if (col >= SCREEN_WIDTH) break;
            *dst++ = shadow_buffer[row][col].value;
        }
    }
}

void restore_screen_region(int start_row, int start_col, int end_row, int end_col, const uint16_t buffer[])
{
    if (start_row < 0) start_row = 0;
    if (start_col < 0) start_col = 0;

    const uint16_t *src = &buffer[0];
    for (int row = start_row; row <= end_row; row++) {
        if (row >= SCREEN_HEIGHT) break;
        for (int col = start_col; col <= end_col; col++) {
            if (col >= SCREEN_WIDTH) break;
            put_value(row, col, *src++);
        }
    }
}

void print_char(int row, int col, char ch)
{
    if (row < 0 || row >= SCREEN_HEIGHT) return;
    if (col < 0 || col >= SCREEN_WIDTH)  return;

    put_char(row, col, ch, (current_attr & 0x0f) | (shadow_buffer[row][col].attr & 0xf0));
}
