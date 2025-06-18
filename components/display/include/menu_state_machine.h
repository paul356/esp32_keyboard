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
#include <sys/queue.h>
#include "esp_err.h"

// Forward declare LVGL object type to avoid including LVGL headers
typedef struct _lv_obj_t lv_obj_t;

/**
 * @brief Input events for menu navigation
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
 * @brief Menu item structure representing nodes in the menu tree
 * Each menu_item represents a state in the state machine
 */
struct menu_item {
    char *text;                                         // Display text for this menu item (dynamically allocated)
    lv_obj_t *icon;                                     // Optional icon (can be NULL)
    esp_err_t (*prepare_gui_func)(struct menu_item *self);  // Optional preparation function
    esp_err_t (*user_action)(struct menu_item *self);   // Optional user action function
    esp_err_t (*post_gui_func)(struct menu_item *self); // Optional long press action function
    void *user_ctx;                                     // User context data
    struct menu_item *parent;                           // Parent menu item
    LIST_ENTRY(menu_item) entry;                        // List entry for siblings
    LIST_HEAD(menu_item_children, menu_item) children;  // List of child menu items
    struct menu_item *focused_child;                    // Currently focused child
};

/**
 * @brief Menu state machine context
 */
typedef struct {
    struct menu_item *root_menu;        // Root menu item (top level)
    struct menu_item *current_menu;     // Current active menu item
    struct menu_item *keyboard_info;    // Keyboard mode menu item (default state)
    uint32_t last_activity_time;        // Last activity timestamp
    uint32_t timeout_ms;                // Timeout in milliseconds
} menu_context_t;

/**
 * @brief Initialize menu state machine with tree structure
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
 * @brief Get current menu item (state)
 * @return Pointer to current menu item
 */
struct menu_item* menu_state_get_current(void);

/**
 * @brief Force return to keyboard mode (default state)
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

/**
 * @brief Create a new menu item
 * @param text Display text for the menu item
 * @param prepare_func Optional preparation function called when entering this menu
 * @param user_action Optional action function called when ENTER is pressed
 * @param post_func Optional cleanup function called when leaving this menu
 * @return Pointer to created menu item, NULL on failure
 */
struct menu_item* menu_item_create(const char *text,
                                  esp_err_t (*prepare_func)(struct menu_item *self),
                                  esp_err_t (*user_action)(struct menu_item *self),
                                  esp_err_t (*post_func)(struct menu_item *self));

/**
 * @brief Add a child menu item to a parent
 * @param parent Parent menu item
 * @param child Child menu item to add
 * @return true on success, false on failure
 */
bool menu_item_add_child(struct menu_item *parent, struct menu_item *child);

/**
 * @brief Set an icon for a menu item
 * @param item Menu item to set icon for
 * @param icon LVGL object representing the icon
 */
void menu_item_set_icon(struct menu_item *item, lv_obj_t *icon);

/**
 * @brief Navigate to a specific menu item
 * @param target Target menu item to navigate to
 * @return true on success, false on failure
 */
bool menu_navigate_to(struct menu_item *target);

/**
 * @brief Free memory allocated for a menu item and its children recursively
 * @param item Menu item to free (can be NULL)
 */
void menu_item_destroy(struct menu_item *item);

// Action functions for menu items
esp_err_t action_bluetooth_toggle(struct menu_item *self);
esp_err_t action_bluetooth_pair_keyboard(struct menu_item *self);
esp_err_t action_bluetooth_pair_admin(struct menu_item *self);
esp_err_t action_wifi_toggle(struct menu_item *self);
esp_err_t action_wifi_settings(struct menu_item *self);
esp_err_t action_led_toggle(struct menu_item *self);
esp_err_t action_led_pattern(struct menu_item *self);
esp_err_t action_keyboard_lock_toggle(struct menu_item *self);
esp_err_t action_input_logging_toggle(struct menu_item *self);
esp_err_t action_standup_reminder_toggle(struct menu_item *self);
esp_err_t action_show_about(struct menu_item *self);

// Prepare functions for menu items
esp_err_t prepare_main_menu(struct menu_item *self);
esp_err_t prepare_bluetooth_menu(struct menu_item *self);
esp_err_t prepare_wifi_menu(struct menu_item *self);
esp_err_t prepare_led_menu(struct menu_item *self);
esp_err_t prepare_advanced_menu(struct menu_item *self);
