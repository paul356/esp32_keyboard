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
#include "input_events.h"

// Forward declare LVGL object type to avoid including LVGL headers
typedef struct _lv_obj_t lv_obj_t;

/**
 * @brief Menu item structure representing nodes in the menu tree
 * Each menu_item represents a state in the state machine
 */
struct menu_item {
    char *text;                                         // Display text for this menu item (dynamically allocated)
    lv_obj_t *icon;                                     // Optional icon (can be NULL)
    esp_err_t (*prepare_gui_func)(struct menu_item *self);  // Optional preparation function
    esp_err_t (*post_gui_func)(struct menu_item *self); // Optional long press action function
    bool (*handle_input_key)(uint8_t key_code);         // Optional input key handler function
    esp_err_t (*user_action)(struct menu_item *self);   // Optional user action function
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
    bool menu_active;                   // Flag indicating if menu is active
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
bool menu_state_process_event(input_event_e event, unsigned char ch);

/**
 * @brief Get current menu item (state)
 * @return Pointer to current menu item
 */
struct menu_item* menu_state_get_current(void);

/**
 * @brief Check if menu state machine is accepting input keys
 * @return true if input keys are accepted, false otherwise
 */
bool menu_state_accept_keys(void);

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
 * @param post_func Optional cleanup function called when leaving this menu
 * @param handle_input_key Optional input key handler function
 * @param user_action Optional action function called when ENTER is pressed
 * @return Pointer to created menu item, NULL on failure
 */
struct menu_item* menu_item_create(const char *text,
                                  esp_err_t (*prepare_func)(struct menu_item *self),
                                  esp_err_t (*post_func)(struct menu_item *self),
                                  bool (*handle_input_key)(uint8_t key_code),
                                  esp_err_t (*user_action)(struct menu_item *self));

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
 * @brief Free memory allocated for a menu item and its children recursively
 * @param item Menu item to free (can be NULL)
 */
void menu_item_destroy(struct menu_item *item);
