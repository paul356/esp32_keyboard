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

#include "menu_state_machine.h"

/**
 * @brief Update keyboard statistics
 */
void keyboard_gui_update_stats(uint32_t count);

/**
 * @brief Initialize keyboard statistics
 */
void keyboard_gui_init_keyboard_stats(void);

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

/**
 * @brief Prepare reset meter GUI function for menu items
 * Creates and shows reset meter interface with banner and reset functionality
 * @param self Menu item that handles reset meter function
 * @return esp_err_t ESP_OK on success
 */
esp_err_t keyboard_gui_prepare_reset_meter(struct menu_item *self);

/**
 * @brief Post reset meter GUI function for menu items
 * Cleanup function for reset meter interface
 * @param self Menu item that had reset meter interface
 * @return esp_err_t ESP_OK on success
 */
esp_err_t keyboard_gui_post_reset_meter(struct menu_item *self);

/**
 * @brief Reset meter user action function
 * This function is called when the user presses Enter on the reset meter menu item
 * @param self Menu item that triggered the action
 * @return esp_err_t ESP_OK on success
 */
esp_err_t keyboard_gui_reset_meter_action(struct menu_item *self);

/**
 * @brief Prepare bt_toggle GUI function for menu items
 * Creates and shows Bluetooth toggle interface with icon and dynamic label
 * @param self Menu item that displays Bluetooth toggle functionality
 * @return esp_err_t ESP_OK on success
 */
esp_err_t keyboard_gui_prepare_bt_toggle(struct menu_item *self);

/**
 * @brief Post bt_toggle GUI function for menu items
 * Cleanup function for Bluetooth toggle interface
 * @param self Menu item that had Bluetooth toggle interface
 * @return esp_err_t ESP_OK on success
 */
esp_err_t keyboard_gui_post_bt_toggle(struct menu_item *self);