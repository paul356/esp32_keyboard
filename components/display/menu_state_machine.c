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

#include <string.h>
#include <stdlib.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "hal_ble.h"
#include "menu_state_machine.h"
#include "keyboard_gui_widgets.h"
#include "keyboard_gui_internal.h"
#include "menu_icons.h"

static const char *TAG = "menu_state";

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
    s_menu_context.timeout_ms = 5000; // 5 seconds

    // Setup the menu tree
    menu_setup_tree();

    // set current_menu to NULL initially
    s_menu_context.current_menu = NULL;
    s_menu_context.menu_active = false;

    ESP_LOGI(TAG, "Menu state machine initialized with tree structure");
    return true;
}

bool menu_state_accept_keys(void)
{
    return s_menu_context.menu_active;
}

bool menu_state_process_event(input_event_e event, unsigned char ch)
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
        if (s_menu_context.menu_active) {
            // If menu is active, focus next child
            menu_focus_next_child(s_menu_context.current_menu);
        } else {
            // If not active, return to root
            menu_navigate_to(s_menu_context.root_menu);
        }
        event_consumed = true;
        break;

    case INPUT_EVENT_ENCODER_CCW:
        if (s_menu_context.menu_active) {
            // If menu is active, focus previous child
            menu_focus_prev_child(s_menu_context.current_menu);
        } else {
            // If not active, return to root
            menu_navigate_to(s_menu_context.root_menu);
        }
        event_consumed = true;
        break;

    case INPUT_EVENT_ENTER:
        // Navigate down the tree or execute action
        if (s_menu_context.current_menu->focused_child)
        {
            struct menu_item *target = s_menu_context.current_menu->focused_child;

            menu_navigate_to(target);
        }
        else if (s_menu_context.current_menu->user_action)
        {
            // Current menu item has an action
            ESP_LOGI(TAG, "Executing action for current menu: %s", s_menu_context.current_menu->text);
            s_menu_context.current_menu->user_action(s_menu_context.current_menu->user_ctx);
        }
        event_consumed = true;
        break;

    case INPUT_EVENT_ESC:
        // Navigate up the tree or do nothing if at root
        if (s_menu_context.current_menu->parent)
        {
            menu_navigate_to(s_menu_context.current_menu->parent);
        } else {
            menu_return_to_keyboard_mode();
        }
        event_consumed = true;
        break;

    case INPUT_EVENT_KEYCODE:
    case INPUT_EVENT_BACKSPACE:
    case INPUT_EVENT_TAB:
    case INPUT_EVENT_RIGHT_ARROW:
    case INPUT_EVENT_LEFT_ARROW:
    case INPUT_EVENT_DOWN_ARROW:
    case INPUT_EVENT_UP_ARROW:
        if (s_menu_context.current_menu->handle_input_key)
        {
            // Handle key input if defined
            s_menu_context.current_menu->handle_input_key(s_menu_context.current_menu->user_ctx, event, ch);
        }
        event_consumed = true;
        break;

    case INPUT_EVENT_TIMEOUT:
        // Return to keyboard mode on timeout
        menu_return_to_keyboard_mode();
        event_consumed = true;
        break;

    default:
        break;
    }

    return event_consumed;
}

struct menu_item* menu_state_get_current_menu(void)
{
    return s_menu_context.current_menu;
}

uint32_t menu_state_get_timeout_ms(void)
{
    return s_menu_context.timeout_ms;
}

void menu_state_set_timeout_ms(uint32_t timeout_ms)
{
    s_menu_context.timeout_ms = timeout_ms;
}

void menu_state_update_timeout(void)
{
    // Only apply timeout when not in keyboard mode
    if (s_menu_context.current_menu == s_menu_context.keyboard_info || s_menu_context.timeout_ms == 0) {
        return;
    }

    uint32_t current_time = esp_timer_get_time() / 1000; // Convert to ms
    if (current_time - s_menu_context.last_activity_time > s_menu_context.timeout_ms) {
        menu_state_process_event(INPUT_EVENT_TIMEOUT, 0);
    }
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

    if (parent->prepare_gui_func) {
        parent->prepare_gui_func(parent);
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

    if (parent->prepare_gui_func) {
        parent->prepare_gui_func(parent);
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
                                  const lv_image_dsc_t *icon,
                                  esp_err_t (*prepare_gui_func)(struct menu_item *self),
                                  esp_err_t (*post_gui_func)(struct menu_item *self),
                                  bool (*handle_input_key)(void *user_ctx, input_event_e input_event, char key_code),
                                  esp_err_t (*user_action)(void *user_ctx))
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
    item->icon = icon;  // Store the icon descriptor
    item->prepare_gui_func = prepare_gui_func;
    item->post_gui_func = post_gui_func;
    item->handle_input_key = handle_input_key;
    item->user_action = user_action;
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

    // Add to parent's children list (at the end)
    if (LIST_EMPTY(&parent->children)) {
        // First child - use INSERT_HEAD
        LIST_INSERT_HEAD(&parent->children, child, entry);
    } else {
        // Find the last child and insert after it
        struct menu_item *last_child = menu_get_last_child(parent);
        LIST_INSERT_AFTER(last_child, child, entry);
    }

    // If this is the first child, set it as focused
    if (!parent->focused_child) {
        parent->focused_child = child;
    }

    ESP_LOGD(TAG, "Added child '%s' to parent '%s'", child->text, parent->text);
    return true;
}

void menu_navigate_to(struct menu_item *target)
{
    if (!target) {
        ESP_LOGE(TAG, "Cannot navigate to NULL menu item");
        return;
    }

    // Call post_gui_func of current menu before navigating
    if (s_menu_context.current_menu && s_menu_context.current_menu->post_gui_func) {
        s_menu_context.current_menu->post_gui_func(s_menu_context.current_menu);
    }

    s_menu_context.current_menu = target;

    // Call prepare function if available
    if (target->prepare_gui_func) {
        target->prepare_gui_func(target);
    }

    if (s_menu_context.current_menu == s_menu_context.keyboard_info)
    {
        s_menu_context.menu_active = false;
    } else {
        s_menu_context.menu_active = true;
    }
    ESP_LOGI(TAG, "Navigated to menu item: %s", target->text);
}

void menu_return_to_keyboard_mode(void)
{
    menu_navigate_to(s_menu_context.keyboard_info);
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

static void prompt_for_passkey_callback(void *arg, enum passkey_event_e event, esp_bd_addr_t bd_addr)
{
    ESP_LOGI(TAG, "Passkey event: %d", event);

    // post a event to update the gui
    keyboard_gui_post_prompt_for_passkey_event((struct menu_item *)arg, event, bd_addr);
}

// Setup the complete menu tree structure
static void menu_setup_tree(void)
{
    ESP_LOGI(TAG, "Setting up menu tree structure");

    // Create root menu (main menu) - nonleaf item that shows menu interface
    s_menu_context.root_menu = menu_item_create("Main Menu", NULL, keyboard_gui_prepare_nonleaf_item, keyboard_gui_post_nonleaf_item, NULL, NULL);

    // Create main menu children: Keyboard Mode, Bluetooth, WiFi, LED, Advanced
    struct menu_item *keyboard_mode_menu = menu_item_create("Keyboard Mode", &keyboard_icon, keyboard_gui_prepare_nonleaf_item, keyboard_gui_post_nonleaf_item, NULL, NULL);
    struct menu_item *bluetooth_menu = menu_item_create("Bluetooth", &bluetooth_icon, keyboard_gui_prepare_nonleaf_item, keyboard_gui_post_nonleaf_item, NULL, NULL);
    struct menu_item *wifi_menu = menu_item_create("WiFi", &wifi_icon, keyboard_gui_prepare_nonleaf_item, keyboard_gui_post_nonleaf_item, NULL, NULL);
    struct menu_item *led_menu = menu_item_create("LED", &led_icon, keyboard_gui_prepare_nonleaf_item, keyboard_gui_post_nonleaf_item, NULL, NULL);
    //struct menu_item *advanced_menu = menu_item_create("Advanced", &advanced_icon, keyboard_gui_prepare_nonleaf_item, keyboard_gui_post_nonleaf_item, NULL, NULL);

    // Create About menu item (to be added to main menu)
    struct menu_item *about = menu_item_create("About", &info_icon, keyboard_gui_prepare_about, keyboard_gui_post_about, NULL, NULL);

    // Add main menu children
    menu_item_add_child(s_menu_context.root_menu, keyboard_mode_menu);
    menu_item_add_child(s_menu_context.root_menu, bluetooth_menu);
    menu_item_add_child(s_menu_context.root_menu, wifi_menu);
    menu_item_add_child(s_menu_context.root_menu, led_menu);
    //menu_item_add_child(s_menu_context.root_menu, advanced_menu);
    menu_item_add_child(s_menu_context.root_menu, about);

    // Create keyboard mode submenu items
    struct menu_item *keyboard_reset_meter = menu_item_create("Reset Meter", &keyboard_meter_reset_icon, keyboard_gui_prepare_reset_meter, keyboard_gui_post_reset_meter, NULL, keyboard_gui_reset_meter_action);
    // Create keyboard mode (default state) - shows keyboard info
    s_menu_context.keyboard_info = menu_item_create("Keyboard Meter", &keyboard_meter_icon, keyboard_gui_prepare_keyboard_info, keyboard_gui_post_keyboard_info, NULL, NULL);
    menu_item_add_child(keyboard_mode_menu, s_menu_context.keyboard_info);
    menu_item_add_child(keyboard_mode_menu, keyboard_reset_meter);

    // Create Bluetooth submenu items
    struct menu_item *bt_toggle = menu_item_create("Toggle Bluetooth", &switch_icon, keyboard_gui_prepare_bt_toggle, keyboard_gui_post_bt_toggle, NULL, keyboard_gui_bt_toggle_action);
    struct menu_item *bt_pair_kb = menu_item_create("Pair Keyboard", &bluetooth_pc_pair, keyboard_gui_prepare_bt_pair_kb, keyboard_gui_post_bt_pair_kb, keyboard_gui_bt_pair_kb_handle_input, keyboard_gui_bt_pair_kb_action);

    menu_item_add_child(bluetooth_menu, bt_toggle);
    menu_item_add_child(bluetooth_menu, bt_pair_kb);

    ble_gap_set_passkey_callback(&prompt_for_passkey_callback, bt_pair_kb);

    // Create WiFi submenu items
    struct menu_item *wifi_toggle = menu_item_create("Toggle WiFi", &switch_icon, keyboard_gui_prepare_wifi_toggle, keyboard_gui_post_wifi_toggle, NULL, keyboard_gui_wifi_toggle_action);
    struct menu_item *wifi_settings = menu_item_create("WiFi Settings", &wifi_setting_icon, keyboard_gui_prepare_wifi_settings, keyboard_gui_post_wifi_settings, keyboard_gui_wifi_settings_handle_input, keyboard_gui_wifi_settings_action);

    menu_item_add_child(wifi_menu, wifi_toggle);
    menu_item_add_child(wifi_menu, wifi_settings);

    // Create LED submenu items
    struct menu_item *led_toggle = menu_item_create("Toggle LED", &switch_icon, keyboard_gui_prepare_led_toggle, keyboard_gui_post_led_toggle, NULL, keyboard_gui_led_toggle_action);
    struct menu_item *led_pattern = menu_item_create("LED Pattern", &led_pattern_icon, keyboard_gui_prepare_led_pattern_settings, keyboard_gui_post_led_pattern_settings, keyboard_gui_led_pattern_settings_handle_input, keyboard_gui_led_pattern_settings_action);

    menu_item_add_child(led_menu, led_toggle);
    menu_item_add_child(led_menu, led_pattern);

    // Implement advanced function later
    //struct menu_item *kb_lock = menu_item_create("Keyboard Lock", &keyboard_lock_icon, NULL, NULL, NULL, NULL);
    //struct menu_item *input_log = menu_item_create("Input Logging", &keyboard_logging_icon, NULL, NULL, NULL, NULL);

    //menu_item_add_child(advanced_menu, kb_lock);
    //menu_item_add_child(advanced_menu, input_log);

    ESP_LOGI(TAG, "Menu tree structure setup complete");
}
