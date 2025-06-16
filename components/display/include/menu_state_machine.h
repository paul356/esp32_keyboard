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

/**
 * @brief Menu states
 */
typedef enum {
    MENU_STATE_KEYBOARD_MODE = 0,
    MENU_STATE_MAIN_MENU,
    MENU_STATE_BLUETOOTH_MENU,
    MENU_STATE_WIFI_MENU,
    MENU_STATE_LED_MENU,
    MENU_STATE_ADVANCED_MENU,
    MENU_STATE_ABOUT_MENU,
    MENU_STATE_MAX
} menu_state_t;

/**
 * @brief Input events
 */
typedef enum {
    INPUT_EVENT_NONE = 0,
    INPUT_EVENT_ENCODER_CW,      // Rotate clockwise
    INPUT_EVENT_ENCODER_CCW,     // Rotate counter-clockwise
    INPUT_EVENT_ENTER,           // Enter key pressed
    INPUT_EVENT_ESC,             // ESC key pressed
    INPUT_EVENT_TIMEOUT,         // Timeout to return to keyboard mode
    INPUT_EVENT_MAX
} input_event_t;

/**
 * @brief Menu item structure
 */
typedef struct menu_item {
    const char *text;
    menu_state_t target_state;
    bool (*action_func)(void);   // Optional action function
    struct menu_item *next;
    struct menu_item *prev;
} menu_item_t;

/**
 * @brief Menu definition structure
 */
typedef struct {
    menu_state_t state;
    const char *title;
    menu_item_t *items;
    menu_item_t *current_item;
    menu_state_t parent_state;
    int item_count;
} menu_def_t;

/**
 * @brief Menu state machine context
 */
typedef struct {
    menu_state_t current_state;
    menu_state_t previous_state;
    menu_def_t *menus[MENU_STATE_MAX];
    int current_selection;
    bool menu_active;
    uint32_t last_activity_time;
    uint32_t timeout_ms;
} menu_context_t;

/**
 * @brief Initialize menu state machine
 * @return true on success, false on failure
 */
bool menu_state_init(void);

/**
 * @brief Process input event
 * @param event Input event to process
 * @return true if event was consumed, false otherwise
 */
bool menu_state_process_event(input_event_t event);

/**
 * @brief Get current menu state
 * @return Current menu state
 */
menu_state_t menu_state_get_current(void);

/**
 * @brief Check if menu is currently active
 * @return true if menu is active, false if in keyboard mode
 */
bool menu_state_is_active(void);

/**
 * @brief Get current menu definition
 * @return Pointer to current menu definition, NULL if in keyboard mode
 */
menu_def_t* menu_state_get_current_menu(void);

/**
 * @brief Force return to keyboard mode
 */
void menu_state_return_to_keyboard(void);

/**
 * @brief Update menu timeout handling
 */
void menu_state_update_timeout(void);

/**
 * @brief Set menu timeout duration
 * @param timeout_ms Timeout in milliseconds (0 to disable)
 */
void menu_state_set_timeout(uint32_t timeout_ms);

// Action functions for menu items
bool action_bluetooth_toggle(void);
bool action_bluetooth_pair_keyboard(void);
bool action_bluetooth_pair_admin(void);
bool action_wifi_toggle(void);
bool action_wifi_settings(void);
bool action_led_toggle(void);
bool action_led_pattern(void);
bool action_keyboard_lock_toggle(void);
bool action_input_logging_toggle(void);
bool action_standup_reminder_toggle(void);
bool action_show_about(void);
