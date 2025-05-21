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

#ifndef KEYMAP_JSON_H
#define KEYMAP_JSON_H

#include "esp_err.h"

/**
 * @brief Function type for appending strings to a target
 * 
 * @param target Opaque pointer to target (e.g., httpd_req_t*, buffer, etc.)
 * @param str String to append
 */
typedef void (*append_str_fn_t)(void* target, const char* str);

/**
 * @brief Generate JSON representation of keyboard layouts
 * 
 * @param append_str Function pointer to append string to target
 * @param target Opaque pointer passed to append_str function
 * @return esp_err_t ESP_OK on success, error code on failure
 */
esp_err_t generate_layouts_json(append_str_fn_t append_str, void* target);

/**
 * @brief Generate JSON representation of available keycodes
 * 
 * @param append_str Function pointer to append string to target
 * @param target Opaque pointer passed to append_str function
 * @return esp_err_t ESP_OK on success, error code on failure
 */
esp_err_t generate_keycodes_json(append_str_fn_t append_str, void* target);

#endif /* KEYMAP_JSON_H */