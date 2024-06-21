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

#pragma once

#include <stdbool.h>
#include "esp_err.h"
#include "quantum_keycodes.h"

#define MACRO_CODE_NUM 32
#define MACRO_CODE_MIN SAFE_RANGE
#define MACRO_CODE_MAX (MACRO_CODE_MIN + MACRO_CODE_NUM - 1)
#define MACRO_RESERVED_MAX (SAFE_RANGE + 256) // MACRO_CODE_NUM is not larger than 256

#define MACRO_STR_MAX_LEN 1024

esp_err_t set_macro_str(uint16_t keycode, char* buf);
esp_err_t get_macro_str(uint16_t keycode, char* buf, int buf_len);

esp_err_t get_macro_name(uint16_t keycode, char* buf, int buf_len);
esp_err_t parse_macro_name(const char* macro_name, uint16_t* keycode);

esp_err_t process_macro_code(uint16_t keycode);
