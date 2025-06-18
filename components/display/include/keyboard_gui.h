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

#include "esp_err.h"
#include "menu_state_machine.h"
#include <stdint.h>

/**
 * @brief Keyboard statistics structure
 */
typedef struct {
    uint32_t total_key_count;
    uint32_t session_key_count;
    uint32_t session_char_count;
    uint32_t session_start_time;
    uint16_t last_key_pressed;
    char last_char_typed;
} keyboard_stats_t;

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
 * @brief Handle rotary encoder rotation
 * @param direction 1 for clockwise, -1 for counter-clockwise
 */
void keyboard_gui_handle_encoder(int direction);

/**
 * @brief Handle Enter key press
 */
void keyboard_gui_handle_enter(void);

/**
 * @brief Handle ESC key press
 */
void keyboard_gui_handle_esc(void);

/**
 * @brief Update keyboard statistics
 * @param keycode The keycode that was pressed
 */
void keyboard_gui_update_stats(uint16_t keycode);

/**
 * @brief Get current keyboard statistics
 * @return Pointer to keyboard statistics structure
 */
keyboard_stats_t* keyboard_gui_get_stats(void);

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

/**
 * @brief Deinitialize keyboard GUI
 * @return esp_err_t ESP_OK on success
 */
esp_err_t keyboard_gui_deinit(void);

/**
 * @brief Prepare keyboard info GUI function for menu items
 * Creates and shows keyboard info interface with periodic updates
 * @param self Menu item that displays keyboard statistics
 * @return esp_err_t ESP_OK on success
 */
esp_err_t keyboard_gui_prepare_keyboard_info(struct menu_item *self);

/**
 * @brief Post keyboard info GUI function for menu items
 * Cleanup function for keyboard info interface, stops periodic updates
 * @param self Menu item that displays keyboard statistics
 * @return esp_err_t ESP_OK on success
 */
esp_err_t keyboard_gui_post_keyboard_info(struct menu_item *self);

/**
 * @brief Prepare nonleaf item GUI function for menu items
 * Creates and shows menu interface for the given nonleaf menu item
 * @param self Menu item that needs menu interface
 * @return esp_err_t ESP_OK on success
 */
esp_err_t keyboard_gui_prepare_nonleaf_item(struct menu_item *self);

/**
 * @brief Post nonleaf item GUI function for menu items
 * Cleanup function for nonleaf item interface
 * @param self Menu item that had menu interface
 * @return esp_err_t ESP_OK on success
 */
esp_err_t keyboard_gui_post_nonleaf_item(struct menu_item *self);
