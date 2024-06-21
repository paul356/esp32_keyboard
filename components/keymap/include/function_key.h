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

// function keys follows macros
#include "macros.h"

#define FUNCTION_KEY_MIN MACRO_RESERVED_MAX
#define FUNCTION_KEY_MAX (FUNCTION_KEY_BUTT - 1)
#define FUNCTION_KEY_NUM (FUNCTION_KEY_BUTT - FUNCTION_KEY_MIN)

enum {
    FUNCTION_KEY_DEVICE_INFO = FUNCTION_KEY_MIN,
    FUNCTION_KEY_INTRO,
    FUNCTION_KEY_HOTSPOT,
    FUNCTION_KEY_BUTT
};

const char *get_function_key_str(uint16_t keycode);
esp_err_t parse_function_key_str(const char* str, uint16_t *keycode);
esp_err_t process_function_key(uint16_t keycode);

