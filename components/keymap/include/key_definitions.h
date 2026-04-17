/*
 *  This program is a keyboard firmware for ESP family boards inspired by
 *  MK32 and qmk_firmware.
 *
 *  Copyright (C) 2024 panhao356@gmail.com
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef KEY_DEFINITIONS_H
#define KEY_DEFINITIONS_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "input_events.h"

#define MAX_ARG_NUM 3

// Alphabetic characters (a-z) - indices 0x04-0x1D
#define SCAN_CODE_A 0x04
#define SCAN_CODE_Z 0x1D

// Numeric characters (1-0) - indices 0x1E-0x27
#define SCAN_CODE_1 0x1E
#define SCAN_CODE_0 0x27

// Special control keys - indices 0x28-0x2C
#define SCAN_CODE_ENTER 0x28
#define SCAN_CODE_ESC 0x29
#define SCAN_CODE_BACKSPACE 0x2A
#define SCAN_CODE_TAB 0x2B
#define SCAN_CODE_SPACE 0x2C

// Arrow keys - indices 0x4F-0x52
#define SCAN_CODE_RIGHT 0x4F
#define SCAN_CODE_LEFT 0x50
#define SCAN_CODE_DOWN 0x51
#define SCAN_CODE_UP 0x52

// Punctuation and symbols - indices 0x2D-0x38
#define SCAN_CODE_MINUS 0x2D        // - and _
#define SCAN_CODE_EQUAL 0x2E        // = and +
#define SCAN_CODE_LBRACKET 0x2F     // [ and {
#define SCAN_CODE_RBRACKET 0x30     // ] and }
#define SCAN_CODE_BACKSLASH 0x31    // \ and |
#define SCAN_CODE_HASH_EU 0x32      // Non-US # and ~
#define SCAN_CODE_SEMICOLON 0x33    // ; and :
#define SCAN_CODE_QUOTE 0x34        // ' and "
#define SCAN_CODE_GRAVE 0x35        // ` and ~
#define SCAN_CODE_COMMA 0x36        // , and <
#define SCAN_CODE_DOT 0x37          // . and >
#define SCAN_CODE_SLASH 0x38        // / and ?

// Additional keys - indices 0x64-0x65
#define SCAN_CODE_BACKSLASH_EU 0x64 // Non-US \ and |

typedef enum {
    LAYER_NUM,
    BASIC_CODE,
    MOD_BITS,
    MACRO_CODE,
    FUNCTION_KEY_CODE
} argument_type_e;

typedef struct {
    const char* desc;
    int num_args;
    argument_type_e arg_types[MAX_ARG_NUM];
} funct_desc_t;

const char* get_basic_key_name(uint16_t keycode);
uint16_t get_max_basic_key_code(void);

int get_total_funct_num(void);
esp_err_t get_funct_desc(int idx, funct_desc_t* desc);

int get_mod_bit_num(void);
const char* get_mod_bit_name(int idx);

esp_err_t get_full_key_name(uint16_t keycode, char* buf, int buf_len);

esp_err_t parse_full_key_name(const char* full_name, uint16_t* keycode);

input_event_e scancode_to_input_event(uint8_t scancode);

char scancode_to_printable_char(bool shifted, uint8_t scancode);

#endif
