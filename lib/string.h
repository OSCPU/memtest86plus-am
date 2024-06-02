// SPDX-License-Identifier: GPL-2.0
#ifndef STRING_H
#define STRING_H
/**
 * \file
 *
 * Provides a subset of the functions normally provided by <string.h>.
 *
 *//*
 * Copyright (C) 2020-2022 Martin Whitaker.
 */

/**
 * Convert n to characters in s
 */

char *itoa(int num, char *str);

/**
 * Convert a hex string to the corresponding 32-bit uint value.
 * returns 0 if a non-hex char is found (not 0-9/a-f/A-F).
 */

uint32_t hexstr2int(const char *hexstr);

#endif // STRING_H
