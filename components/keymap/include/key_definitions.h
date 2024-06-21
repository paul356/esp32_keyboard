/*
 *  This program is a keyboard firmware for ESP family boards inspired by
 *  MK32 and qmk_firmware.
 *
 *  Copyright (C) 2024 github.com/paul356
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
#include "esp_err.h"

#define MAX_ARG_NUM 3

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

#endif
