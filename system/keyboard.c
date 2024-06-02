// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2020-2022 Martin Whitaker.

#include <stdint.h>

#include "bootparams.h"

#include "io.h"
#include "usbhcd.h"

#include "serial.h"

#include "keyboard.h"
#include "config.h"
#include <assert.h>
#include <klib-macros.h>

//------------------------------------------------------------------------------
// Private Variables
//------------------------------------------------------------------------------

// Convert set 1 scancodes to characters.
const char legacy_keymap[] = {
    /* 0x00 */   0,
    /* 0x01 */ ESC,
    /* 0x02 */ '1',
    /* 0x03 */ '2',
    /* 0x04 */ '3',
    /* 0x05 */ '4',
    /* 0x06 */ '5',
    /* 0x07 */ '6',
    /* 0x08 */ '7',
    /* 0x09 */ '8',
    /* 0x0a */ '9',
    /* 0x0b */ '0',
    /* 0x0c */ '-',
    /* 0x0d */ '+',
    /* 0x0e */ '\b',
    /* 0x0f */ '\t',
    /* 0x10 */ 'q',
    /* 0x11 */ 'w',
    /* 0x12 */ 'e',
    /* 0x13 */ 'r',
    /* 0x14 */ 't',
    /* 0x15 */ 'y',
    /* 0x16 */ 'u',
    /* 0x17 */ 'i',
    /* 0x18 */ 'o',
    /* 0x19 */ 'p',
    /* 0x1a */ '[',
    /* 0x1b */ ']',
    /* 0x1c */ '\n',
    /* 0x1d */   0,
    /* 0x1e */ 'a',
    /* 0x1f */ 's',
    /* 0x20 */ 'd',
    /* 0x21 */ 'f',
    /* 0x22 */ 'g',
    /* 0x23 */ 'h',
    /* 0x24 */ 'j',
    /* 0x25 */ 'k',
    /* 0x26 */ 'l',
    /* 0x27 */ ';',
    /* 0x28 */ '\'',
    /* 0x29 */ '`',
    /* 0x2a */   0,
    /* 0x2b */ '\\',
    /* 0x2c */ 'z',
    /* 0x2d */ 'x',
    /* 0x2e */ 'c',
    /* 0x2f */ 'v',
    /* 0x30 */ 'b',
    /* 0x31 */ 'n',
    /* 0x32 */ 'm',
    /* 0x33 */ ',',
    /* 0x34 */ '.',
    /* 0x35 */ '/',
    /* 0x36 */   0,
    /* 0x37 */ '*',  // keypad
    /* 0x38 */   0,
    /* 0x39 */ ' ',
    /* 0x3a */   0,
    /* 0x3b */ '1',  // F1
    /* 0x3c */ '2',  // F2
    /* 0x3d */ '3',  // F3
    /* 0x3e */ '4',  // F4
    /* 0x3f */ '5',  // F5
    /* 0x40 */ '6',  // F6
    /* 0x41 */ '7',  // F7
    /* 0x42 */ '8',  // F8
    /* 0x43 */ '9',  // F9
    /* 0x44 */ '0',  // F10
    /* 0x45 */   0,
    /* 0x46 */   0,
    /* 0x47 */   0,  // keypad
    /* 0x48 */ 'u',  // keypad
    /* 0x49 */   0,  // keypad
    /* 0x4a */ '-',  // keypad
    /* 0x4b */ 'l',  // keypad
    /* 0x4c */   0,  // keypad
    /* 0x4d */ 'r',  // keypad
    /* 0x4e */ '+',  // keypad
    /* 0x4f */   0,  // keypad
    /* 0x50 */ 'd',  // keypad
    /* 0x51 */   0,  // keypad
    /* 0x52 */   0,  // keypad
    /* 0x53 */   0,  // keypad
};


//------------------------------------------------------------------------------
// Public Variables
//------------------------------------------------------------------------------

keyboard_types_t keyboard_types = KT_NONE;

//------------------------------------------------------------------------------
// Public Functions
//------------------------------------------------------------------------------

void keyboard_init(void)
{
#if 0
    if (keyboard_types == KT_NONE) {
        // No command line option was found, so set the default according to
        // how we were booted.
        const boot_params_t *boot_params = (boot_params_t *)boot_params_addr;
        if (boot_params->efi_info.loader_signature != 0) {
            keyboard_types = KT_USB|KT_LEGACY;
        } else {
            keyboard_types = KT_LEGACY;
        }
    }
    if (keyboard_types & KT_USB) {
        find_usb_keyboards(keyboard_types == KT_USB);
    }
#endif
    keyboard_types = KT_NONE;
}

char get_key(void)
{
  AM_INPUT_KEYBRD_T ev = io_read(AM_INPUT_KEYBRD);
  if (ev.keycode == AM_KEY_NONE) return 0;
  switch (ev.keycode) {
    case AM_KEY_UP:    return 'u';
    case AM_KEY_DOWN:  return 'd';
    case AM_KEY_LEFT:  return 'l';
    case AM_KEY_RIGHT: return 'r';
    default:
      if (ev.keycode < sizeof(legacy_keymap)) {
          return legacy_keymap[ev.keycode];
      }
  }

    if (enable_tty) {
        uint8_t c = tty_get_key();
        if (c != 0xFF) {
            if (c == 0x0D) c = '\n'; // Enter
            return c;
        }
    }

    return '\0';
}
