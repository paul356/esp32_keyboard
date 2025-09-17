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

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "input_events.h"

/**
 * @brief Initialize keyboard GUI
 * @return esp_err_t ESP_OK on success
 */
esp_err_t keyboard_gui_init(void);

/**
 * @brief Update GUI display
 * This function should be called periodically to update the display
 */
void keyboard_gui_update(void);

/**
 * @brief Handle key input from keyboard
 * This function processes key input and updates the GUI accordingly.
 * @param mods Modifier keys (e.g., Shift, Ctrl)
 * @param scan_code Pointer to array of scan codes
 * @param code_len Length of the scan code array
 * @param nkro_bits When nkro is true, the scan_code array is a bitmap for keycode 0x00 to 0x7f
 * @return bool true if the input was handled, false otherwise
 */
bool keyboard_gui_handle_key_input(uint8_t mods, uint8_t *scan_code, int code_len, bool nkro_bits);

/**
 * @brief Post input event from ISR
 * This function is called from ISR to post input events to the GUI
 * @param event The input event type
 * @param keycode The keycode associated with the event
 * @return esp_err_t ESP_OK on success
 */
esp_err_t keyboard_gui_post_input_event_isr(input_event_e event, char keycode);

/**
 * @brief Reset session statistics
 */
void keyboard_gui_reset_session_stats(void);

/**
 * @brief Set display brightness
 * @param brightness Brightness level (0-100)
 * @return esp_err_t ESP_OK on success
 */
esp_err_t keyboard_gui_set_brightness(uint8_t brightness);

/**
 * @brief Turn display on/off
 * @param on true to turn on, false to turn off
 * @return esp_err_t ESP_OK on success
 */
esp_err_t keyboard_gui_display_on_off(bool on);