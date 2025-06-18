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

#include "menu_state_machine.h"
#include "keyboard_gui.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include <string.h>
#include <stdlib.h>

static const char *TAG = "menu_state";

static menu_context_t s_menu_context = {0};

// Forward declarations
static void menu_setup_tree(void);
static void menu_focus_next_child(struct menu_item *parent);
static void menu_focus_prev_child(struct menu_item *parent);
static struct menu_item* menu_get_first_child(struct menu_item *parent);
static struct menu_item* menu_get_last_child(struct menu_item *parent);
static struct menu_item* menu_get_next_sibling(struct menu_item *item);
static struct menu_item* menu_get_prev_sibling(struct menu_item *item);

bool menu_state_init(void)
{
    ESP_LOGI(TAG, "Initializing menu state machine with tree structure");

    memset(&s_menu_context, 0, sizeof(s_menu_context));
    s_menu_context.timeout_ms = 10000; // 10 seconds

    // Setup the menu tree
    menu_setup_tree();

    // set current_menu to NULL initially, then menu_state_return_to_keyboard will draw the GUI
    s_menu_context.current_menu = NULL;

    ESP_LOGI(TAG, "Menu state machine initialized with tree structure");
    return true;
}

bool menu_state_process_event(input_event_t event)
{
    bool event_consumed = false;
    s_menu_context.last_activity_time = esp_timer_get_time() / 1000; // Convert to ms

    ESP_LOGI(TAG, "Processing event %d in menu item: %s", event,
             s_menu_context.current_menu ? s_menu_context.current_menu->text : "NULL");

    if (!s_menu_context.current_menu) {
        return false;
    }

    // Menu system active: handle navigation
    switch (event)
    {
    case INPUT_EVENT_ENCODER_CW:
        // Move to next child
        menu_focus_next_child(s_menu_context.current_menu);
        event_consumed = true;
        break;

    case INPUT_EVENT_ENCODER_CCW:
        // Move to previous child
        menu_focus_prev_child(s_menu_context.current_menu);
        event_consumed = true;
        break;

    case INPUT_EVENT_ENTER:
        // Navigate down the tree or execute action
        if (s_menu_context.current_menu->focused_child)
        {
            struct menu_item *target = s_menu_context.current_menu->focused_child;

            // Check if this is a leaf node (has user_action) or has children
            if (!LIST_EMPTY(&target->children))
            {
                // Non-leaf node: navigate down
                // Call post_gui_func of current menu before navigating
                if (s_menu_context.current_menu->post_gui_func)
                {
                    s_menu_context.current_menu->post_gui_func(s_menu_context.current_menu);
                }

                s_menu_context.current_menu = target;

                // Call prepare function if available
                if (target->prepare_gui_func)
                {
                    target->prepare_gui_func(target);
                }

                ESP_LOGI(TAG, "Navigated to: %s", target->text);
            }
            else if (target->user_action)
            {
                // Leaf node: execute action
                ESP_LOGI(TAG, "Executing action for: %s", target->text);
                target->user_action(target);
            }
        }
        else if (s_menu_context.current_menu->user_action)
        {
            // Current menu item has an action
            ESP_LOGI(TAG, "Executing action for current menu: %s", s_menu_context.current_menu->text);
            s_menu_context.current_menu->user_action(s_menu_context.current_menu);
        }
        event_consumed = true;
        break;

    case INPUT_EVENT_ESC:
        // Navigate up the tree or do nothing if at root
        if (s_menu_context.current_menu->parent)
        {
            // Call post_gui_func of current menu before navigating up
            if (s_menu_context.current_menu->post_gui_func)
            {
                s_menu_context.current_menu->post_gui_func(s_menu_context.current_menu);
            }

            s_menu_context.current_menu = s_menu_context.current_menu->parent;

            // Call prepare_gui_func of the parent menu
            if (s_menu_context.current_menu->prepare_gui_func)
            {
                s_menu_context.current_menu->prepare_gui_func(s_menu_context.current_menu);
            }

            ESP_LOGI(TAG, "Navigated up to: %s", s_menu_context.current_menu->text);
        }
        event_consumed = true;
        break;

    case INPUT_EVENT_TIMEOUT:
        // Return to keyboard mode on timeout
        menu_state_return_to_keyboard();
        event_consumed = true;
        break;

    default:
        break;
    }

    return event_consumed;
}

struct menu_item* menu_state_get_current(void)
{
    return s_menu_context.current_menu;
}

void menu_state_return_to_keyboard(void)
{
    // Check if we're already in keyboard mode
    if (s_menu_context.current_menu == s_menu_context.keyboard_info) {
        ESP_LOGD(TAG, "Already in keyboard mode, no action needed");
        return;
    }

    // Call post_gui_func of the current menu if it exists
    if (s_menu_context.current_menu && s_menu_context.current_menu->post_gui_func) {
        s_menu_context.current_menu->post_gui_func(s_menu_context.current_menu);
    }

    // Set current menu to keyboard info
    s_menu_context.current_menu = s_menu_context.keyboard_info;

    // Call prepare_gui_func to display keyboard info
    if (s_menu_context.current_menu && s_menu_context.current_menu->prepare_gui_func) {
        s_menu_context.current_menu->prepare_gui_func(s_menu_context.current_menu);
    }

    ESP_LOGI(TAG, "Returned to keyboard mode");
}

void menu_state_update_timeout(void)
{
    // Only apply timeout when not in keyboard mode
    if (s_menu_context.current_menu == s_menu_context.keyboard_info || s_menu_context.timeout_ms == 0) {
        return;
    }

    uint32_t current_time = esp_timer_get_time() / 1000; // Convert to ms
    if (current_time - s_menu_context.last_activity_time > s_menu_context.timeout_ms) {
        menu_state_process_event(INPUT_EVENT_TIMEOUT);
    }
}

void menu_state_set_timeout(uint32_t timeout_ms)
{
    s_menu_context.timeout_ms = timeout_ms;
    ESP_LOGI(TAG, "Menu timeout set to %lu ms", timeout_ms);
}

esp_err_t action_bluetooth_toggle(struct menu_item *self)
{
    ESP_LOGI(TAG, "Toggling Bluetooth");
    // TODO: Implement actual Bluetooth toggle functionality
    return ESP_OK;
}

esp_err_t action_bluetooth_pair_keyboard(struct menu_item *self)
{
    ESP_LOGI(TAG, "Pairing keyboard via Bluetooth");
    // TODO: Implement Bluetooth keyboard pairing
    return ESP_OK;
}

esp_err_t action_bluetooth_pair_admin(struct menu_item *self)
{
    ESP_LOGI(TAG, "Pairing admin device via Bluetooth");
    // TODO: Implement Bluetooth admin device pairing
    return ESP_OK;
}

esp_err_t action_wifi_toggle(struct menu_item *self)
{
    ESP_LOGI(TAG, "Toggling WiFi");
    // TODO: Implement actual WiFi toggle functionality
    return ESP_OK;
}

esp_err_t action_wifi_settings(struct menu_item *self)
{
    ESP_LOGI(TAG, "Opening WiFi settings");
    // TODO: Implement WiFi settings menu
    return ESP_OK;
}

esp_err_t action_led_toggle(struct menu_item *self)
{
    ESP_LOGI(TAG, "Toggling LED");
    // TODO: Implement LED toggle functionality
    return ESP_OK;
}

esp_err_t action_led_pattern(struct menu_item *self)
{
    ESP_LOGI(TAG, "Changing LED pattern");
    // TODO: Implement LED pattern change
    return ESP_OK;
}

esp_err_t action_keyboard_lock_toggle(struct menu_item *self)
{
    ESP_LOGI(TAG, "Toggling keyboard lock");
    // TODO: Implement keyboard lock toggle
    return ESP_OK;
}

esp_err_t action_input_logging_toggle(struct menu_item *self)
{
    ESP_LOGI(TAG, "Toggling input logging");
    // TODO: Implement input logging toggle
    return ESP_OK;
}

esp_err_t action_standup_reminder_toggle(struct menu_item *self)
{
    ESP_LOGI(TAG, "Toggling standup reminder");
    // TODO: Implement standup reminder toggle
    return ESP_OK;
}

esp_err_t action_show_about(struct menu_item *self)
{
    ESP_LOGI(TAG, "Showing about information");
    // TODO: Implement about information display
    return ESP_OK;
}

// Prepare function implementations
esp_err_t prepare_main_menu(struct menu_item *self)
{
    ESP_LOGD(TAG, "Preparing main menu");
    // TODO: Update main menu display, refresh status information
    return ESP_OK;
}

esp_err_t prepare_bluetooth_menu(struct menu_item *self)
{
    ESP_LOGD(TAG, "Preparing Bluetooth menu");
    // TODO: Update Bluetooth status, scan for devices, etc.
    return ESP_OK;
}

esp_err_t prepare_wifi_menu(struct menu_item *self)
{
    ESP_LOGD(TAG, "Preparing WiFi menu");
    // TODO: Update WiFi status, scan for networks, etc.
    return ESP_OK;
}

esp_err_t prepare_led_menu(struct menu_item *self)
{
    ESP_LOGD(TAG, "Preparing LED menu");
    // TODO: Update LED status display
    return ESP_OK;
}

esp_err_t prepare_advanced_menu(struct menu_item *self)
{
    ESP_LOGD(TAG, "Preparing advanced menu");
    // TODO: Update advanced settings display
    return ESP_OK;
}

// Helper functions for menu navigation
static void menu_focus_next_child(struct menu_item *parent)
{
    if (!parent || LIST_EMPTY(&parent->children)) {
        return;
    }

    if (!parent->focused_child) {
        parent->focused_child = menu_get_first_child(parent);
    } else {
        struct menu_item *next = menu_get_next_sibling(parent->focused_child);
        if (next) {
            parent->focused_child = next;
        } else {
            // Wrap around to first child
            parent->focused_child = menu_get_first_child(parent);
        }
    }

    ESP_LOGD(TAG, "Next child focused: %s", parent->focused_child ? parent->focused_child->text : "NULL");
}

static void menu_focus_prev_child(struct menu_item *parent)
{
    if (!parent || LIST_EMPTY(&parent->children)) {
        return;
    }

    if (!parent->focused_child) {
        parent->focused_child = menu_get_last_child(parent);
    } else {
        struct menu_item *prev = menu_get_prev_sibling(parent->focused_child);
        if (prev) {
            parent->focused_child = prev;
        } else {
            // Wrap around to last child
            parent->focused_child = menu_get_last_child(parent);
        }
    }

    ESP_LOGD(TAG, "Previous child focused: %s", parent->focused_child ? parent->focused_child->text : "NULL");
}

static struct menu_item* menu_get_first_child(struct menu_item *parent)
{
    if (!parent || LIST_EMPTY(&parent->children)) {
        return NULL;
    }
    return LIST_FIRST(&parent->children);
}

static struct menu_item* menu_get_last_child(struct menu_item *parent)
{
    if (!parent || LIST_EMPTY(&parent->children)) {
        return NULL;
    }

    struct menu_item *item = LIST_FIRST(&parent->children);
    struct menu_item *last = item;

    LIST_FOREACH(item, &parent->children, entry) {
        last = item;
    }

    return last;
}

static struct menu_item* menu_get_next_sibling(struct menu_item *item)
{
    if (!item) {
        return NULL;
    }
    return LIST_NEXT(item, entry);
}

static struct menu_item* menu_get_prev_sibling(struct menu_item *item)
{
    if (!item || !item->parent) {
        return NULL;
    }

    struct menu_item *current;
    struct menu_item *prev = NULL;

    LIST_FOREACH(current, &item->parent->children, entry) {
        if (current == item) {
            return prev;
        }
        prev = current;
    }

    return NULL;
}

// Public functions for menu item management
struct menu_item* menu_item_create(const char *text,
                                  esp_err_t (*prepare_gui_func)(struct menu_item *self),
                                  esp_err_t (*user_action)(struct menu_item *self),
                                  esp_err_t (*post_gui_func)(struct menu_item *self))
{
    if (!text) {
        ESP_LOGE(TAG, "Cannot create menu item with NULL text");
        return NULL;
    }

    struct menu_item *item = heap_caps_calloc(1, sizeof(struct menu_item), MALLOC_CAP_DEFAULT);
    if (!item) {
        ESP_LOGE(TAG, "Failed to allocate memory for menu item");
        return NULL;
    }

    // Allocate and copy text
    item->text = heap_caps_malloc(strlen(text) + 1, MALLOC_CAP_DEFAULT);
    if (!item->text) {
        ESP_LOGE(TAG, "Failed to allocate memory for menu item text");
        heap_caps_free(item);
        return NULL;
    }
    strcpy(item->text, text);

    // Initialize fields
    item->icon = NULL;
    item->prepare_gui_func = prepare_gui_func;
    item->user_action = user_action;
    item->post_gui_func = post_gui_func;
    item->user_ctx = NULL;  // Removed user_ctx parameter, always set to NULL
    item->parent = NULL;
    item->focused_child = NULL;

    // Initialize children list
    LIST_INIT(&item->children);

    ESP_LOGD(TAG, "Created menu item: %s", text);
    return item;
}

bool menu_item_add_child(struct menu_item *parent, struct menu_item *child)
{
    if (!parent || !child) {
        ESP_LOGE(TAG, "Cannot add child: parent or child is NULL");
        return false;
    }

    if (child->parent) {
        ESP_LOGW(TAG, "Child '%s' already has a parent", child->text);
        return false;
    }

    // Set parent relationship
    child->parent = parent;

    // Add to parent's children list
    LIST_INSERT_HEAD(&parent->children, child, entry);

    // If this is the first child, set it as focused
    if (!parent->focused_child) {
        parent->focused_child = child;
    }

    ESP_LOGD(TAG, "Added child '%s' to parent '%s'", child->text, parent->text);
    return true;
}

void menu_item_set_icon(struct menu_item *item, lv_obj_t *icon)
{
    if (item) {
        item->icon = icon;
        ESP_LOGD(TAG, "Set icon for menu item: %s", item->text);
    }
}

bool menu_navigate_to(struct menu_item *target)
{
    if (!target) {
        ESP_LOGE(TAG, "Cannot navigate to NULL menu item");
        return false;
    }

    // Call post_gui_func of current menu before navigating
    if (s_menu_context.current_menu && s_menu_context.current_menu->post_gui_func) {
        s_menu_context.current_menu->post_gui_func(s_menu_context.current_menu);
    }

    s_menu_context.current_menu = target;
    s_menu_context.last_activity_time = esp_timer_get_time() / 1000; // Convert to ms

    // Call prepare function if available
    if (target->prepare_gui_func) {
        target->prepare_gui_func(target);
    }

    ESP_LOGI(TAG, "Navigated to menu item: %s", target->text);
    return true;
}

void menu_item_destroy(struct menu_item *item)
{
    if (!item) {
        return;
    }

    ESP_LOGD(TAG, "Destroying menu item: %s", item->text);

    // Recursively destroy all children first
    struct menu_item *child, *temp_child;
    LIST_FOREACH_SAFE(child, &item->children, entry, temp_child) {
        LIST_REMOVE(child, entry);
        menu_item_destroy(child);
    }

    // Free the text string
    if (item->text) {
        heap_caps_free(item->text);
    }

    // Free the item itself
    heap_caps_free(item);
}

// Setup the complete menu tree structure
static void menu_setup_tree(void)
{
    ESP_LOGI(TAG, "Setting up menu tree structure");

    // Create keyboard mode (default state) - shows keyboard info
    s_menu_context.keyboard_info = menu_item_create("Keyboard Mode", keyboard_gui_prepare_keyboard_info, NULL, keyboard_gui_post_keyboard_info);

    // Create root menu (main menu) - nonleaf item that shows menu interface
    s_menu_context.root_menu = menu_item_create("Main Menu", keyboard_gui_prepare_nonleaf_item, NULL, keyboard_gui_post_nonleaf_item);

    // Create main menu children: Keyboard Mode, Bluetooth, WiFi, LED, Advanced
    struct menu_item *keyboard_mode_menu = s_menu_context.keyboard_info;
    struct menu_item *bluetooth_menu = menu_item_create("Bluetooth", keyboard_gui_prepare_nonleaf_item, NULL, keyboard_gui_post_nonleaf_item);
    struct menu_item *wifi_menu = menu_item_create("WiFi", keyboard_gui_prepare_nonleaf_item, NULL, keyboard_gui_post_nonleaf_item);
    struct menu_item *led_menu = menu_item_create("LED", keyboard_gui_prepare_nonleaf_item, NULL, keyboard_gui_post_nonleaf_item);
    struct menu_item *advanced_menu = menu_item_create("Advanced", keyboard_gui_prepare_nonleaf_item, NULL, keyboard_gui_post_nonleaf_item);

    // Add main menu children
    menu_item_add_child(s_menu_context.root_menu, keyboard_mode_menu);
    menu_item_add_child(s_menu_context.root_menu, bluetooth_menu);
    menu_item_add_child(s_menu_context.root_menu, wifi_menu);
    menu_item_add_child(s_menu_context.root_menu, led_menu);
    menu_item_add_child(s_menu_context.root_menu, advanced_menu);

    // Create Bluetooth submenu items
    struct menu_item *bt_toggle = menu_item_create("Toggle Bluetooth", NULL, action_bluetooth_toggle, NULL);
    struct menu_item *bt_pair_kb = menu_item_create("Pair Keyboard", NULL, action_bluetooth_pair_keyboard, NULL);
    struct menu_item *bt_pair_admin = menu_item_create("Pair Admin Device", NULL, action_bluetooth_pair_admin, NULL);

    menu_item_add_child(bluetooth_menu, bt_toggle);
    menu_item_add_child(bluetooth_menu, bt_pair_kb);
    menu_item_add_child(bluetooth_menu, bt_pair_admin);

    // Create WiFi submenu items
    struct menu_item *wifi_toggle = menu_item_create("Toggle WiFi", NULL, action_wifi_toggle, NULL);
    struct menu_item *wifi_settings = menu_item_create("WiFi Settings", NULL, action_wifi_settings, NULL);

    menu_item_add_child(wifi_menu, wifi_toggle);
    menu_item_add_child(wifi_menu, wifi_settings);

    // Create LED submenu items
    struct menu_item *led_toggle = menu_item_create("Toggle LED", NULL, action_led_toggle, NULL);
    struct menu_item *led_pattern = menu_item_create("LED Pattern", NULL, action_led_pattern, NULL);

    menu_item_add_child(led_menu, led_toggle);
    menu_item_add_child(led_menu, led_pattern);

    // Create Advanced submenu items
    struct menu_item *kb_lock = menu_item_create("Keyboard Lock", NULL, action_keyboard_lock_toggle, NULL);
    struct menu_item *input_log = menu_item_create("Input Logging", NULL, action_input_logging_toggle, NULL);
    struct menu_item *standup_reminder = menu_item_create("Standup Reminder", NULL, action_standup_reminder_toggle, NULL);
    struct menu_item *about = menu_item_create("About", NULL, action_show_about, NULL);

    menu_item_add_child(advanced_menu, kb_lock);
    menu_item_add_child(advanced_menu, input_log);
    menu_item_add_child(advanced_menu, standup_reminder);
    menu_item_add_child(advanced_menu, about);

    ESP_LOGI(TAG, "Menu tree structure setup complete");
}
