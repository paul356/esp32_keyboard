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

#include "hal_ble.h"
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
 * @param user_ctx User context data for the action
 * @return esp_err_t ESP_OK on success
 */
esp_err_t keyboard_gui_reset_meter_action(void *user_ctx);

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

/**
 * @brief Prepare wifi_toggle GUI function for menu items
 * Creates and shows WiFi toggle interface with icon and dynamic label
 * @param self Menu item that displays WiFi toggle functionality
 * @return esp_err_t ESP_OK on success
 */
esp_err_t keyboard_gui_prepare_wifi_toggle(struct menu_item *self);

/**
 * @brief Post wifi_toggle GUI function for menu items
 * Cleanup function for WiFi toggle interface
 * @param self Menu item that had WiFi toggle interface
 * @return esp_err_t ESP_OK on success
 */
esp_err_t keyboard_gui_post_wifi_toggle(struct menu_item *self);

/**
 * @brief WiFi toggle user action function
 * This function is called when the user presses Enter on the WiFi toggle menu item
 * Toggles WiFi on/off and updates the display accordingly
 * @param user_ctx User context data for the action
 * @return esp_err_t ESP_OK on success
 */
esp_err_t keyboard_gui_wifi_toggle_action(void *user_ctx);

/**
 * @brief Prepare led_toggle GUI function for menu items
 * Creates and shows LED toggle interface with icon and dynamic label
 * @param self Menu item that displays LED toggle functionality
 * @return esp_err_t ESP_OK on success
 */
esp_err_t keyboard_gui_prepare_led_toggle(struct menu_item *self);

/**
 * @brief Post led_toggle GUI function for menu items
 * Cleanup function for LED toggle interface
 * @param self Menu item that had LED toggle interface
 * @return esp_err_t ESP_OK on success
 */
esp_err_t keyboard_gui_post_led_toggle(struct menu_item *self);

/**
 * @brief LED toggle user action function
 * This function is called when the user presses Enter on the LED toggle menu item
 * Toggles LED on/off and updates the display accordingly
 * @param user_ctx User context data for the action
 * @return esp_err_t ESP_OK on success
 */
esp_err_t keyboard_gui_led_toggle_action(void *user_ctx);

/**
 * @brief Prepare LED pattern settings GUI function for menu items
 * Creates and shows LED pattern settings interface with pattern roller and parameter fields
 * @param self Menu item that displays LED pattern settings
 * @return esp_err_t ESP_OK on success
 */
esp_err_t keyboard_gui_prepare_led_pattern_settings(struct menu_item *self);

/**
 * @brief Post LED pattern settings GUI function for menu items
 * Cleanup function for LED pattern settings interface
 * @param self Menu item that displays LED pattern settings
 * @return esp_err_t ESP_OK on success
 */
esp_err_t keyboard_gui_post_led_pattern_settings(struct menu_item *self);

/**
 * @brief Input handler for LED pattern settings menu item
 * Handles navigation, text input and pattern selection for LED pattern settings
 * @param user_ctx User context data for the input handler
 * @param input_event Type of input event
 * @param key_code Key code for the input
 * @return bool true if input was handled, false otherwise
 */
bool keyboard_gui_led_pattern_settings_handle_input(void *user_ctx, input_event_e input_event, char key_code);

/**
 * @brief Action function for LED pattern settings menu item
 * Applies the current LED pattern settings
 * @param user_ctx User context data for the action function
 * @return esp_err_t ESP_OK on success
 */
esp_err_t keyboard_gui_led_pattern_settings_action(void *user_ctx);

/**
 * @brief Prepare bt_pair_kb GUI function for menu items
 * Creates and shows Bluetooth pair keyboard interface with BLE name and icon
 * @param self Menu item that displays Bluetooth pair keyboard functionality
 * @return esp_err_t ESP_OK on success
 */
esp_err_t keyboard_gui_prepare_bt_pair_kb(struct menu_item *self);

/**
 * @brief Post bt_pair_kb GUI function for menu items
 * Cleanup function for Bluetooth pair keyboard interface
 * @param self Menu item that had Bluetooth pair keyboard interface
 * @return esp_err_t ESP_OK on success
 */
esp_err_t keyboard_gui_post_bt_pair_kb(struct menu_item *self);

/**
 * @brief Input handler for bt_pair_kb menu item
 * Handles numeric input for Bluetooth keyboard pairing PIN entry
 * @param user_ctx User context data for the input handler
 * @param input_event Type of input event
 * @param key_code Key code for the input
 * @return bool true if input was handled, false otherwise
 */
bool keyboard_gui_bt_pair_kb_handle_input(void *user_ctx, input_event_e input_event, char key_code);

void keyboard_gui_bt_pair_kb_prompt_for_passkey(struct menu_item *self, enum passkey_event_e event, esp_bd_addr_t bd_addr);

// Action function to send passkey reply using esp_ble_passkey_reply
esp_err_t keyboard_gui_bt_pair_kb_action(void *user_ctx);

/**
 * @brief Prepare WiFi settings GUI function for menu items
 * Creates and shows WiFi settings interface with mode selector, SSID and password fields
 * @param self Menu item that displays WiFi settings
 * @return esp_err_t ESP_OK on success
 */
esp_err_t keyboard_gui_prepare_wifi_settings(struct menu_item *self);

/**
 * @brief Post WiFi settings GUI function for menu items
 * Cleanup function for WiFi settings interface
 * @param self Menu item that displays WiFi settings
 * @return esp_err_t ESP_OK on success
 */
esp_err_t keyboard_gui_post_wifi_settings(struct menu_item *self);

/**
 * @brief Handle input events for WiFi settings GUI
 * Processes TAB navigation, text input, cursor movement
 * @param user_ctx WiFi settings GUI context
 * @param input_event Input event type
 * @param key_code Character code for text input
 * @return bool true if event was handled, false otherwise
 */
bool keyboard_gui_wifi_settings_handle_input(void *user_ctx, input_event_e input_event, char key_code);

/**
 * @brief WiFi settings action function
 * This function is called when the user presses Enter on the WiFi settings menu item
 * Applies the configured WiFi settings (mode, SSID, password)
 * @param user_ctx WiFi settings GUI context
 * @return esp_err_t ESP_OK on success
 */
esp_err_t keyboard_gui_wifi_settings_action(void *user_ctx);

/**
 * @brief Prepare About GUI function for menu items
 * Creates and shows About screen with keyboard information displayed in the center
 * @param self Menu item that displays About information
 * @return esp_err_t ESP_OK on success
 */
esp_err_t keyboard_gui_prepare_about(struct menu_item *self);

/**
 * @brief Post About GUI function for menu items
 * Cleanup function for About screen interface
 * @param self Menu item that had About interface
 * @return esp_err_t ESP_OK on success
 */
esp_err_t keyboard_gui_post_about(struct menu_item *self);