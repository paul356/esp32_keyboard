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
#include "esp_log.h"
#include "esp_timer.h"
#include <string.h>

static const char *TAG = "menu_state";

static menu_context_t s_menu_context = {0};

// Forward declarations
static void menu_create_main_menu(void);
static void menu_create_bluetooth_menu(void);
static void menu_create_wifi_menu(void);
static void menu_create_led_menu(void);
static void menu_create_advanced_menu(void);
static void menu_create_about_menu(void);
static menu_item_t* menu_create_item(const char *text, menu_state_t target_state, bool (*action_func)(void));
static void menu_add_item(menu_def_t *menu, menu_item_t *item);
static void menu_set_current_selection(menu_def_t *menu, int selection);

// Menu items storage
static menu_item_t main_menu_items[5];
static menu_item_t bluetooth_menu_items[3];
static menu_item_t wifi_menu_items[2];
static menu_item_t led_menu_items[2];
static menu_item_t advanced_menu_items[3];
static menu_item_t about_menu_items[1];

// Menu definitions
static menu_def_t main_menu;
static menu_def_t bluetooth_menu;
static menu_def_t wifi_menu;
static menu_def_t led_menu;
static menu_def_t advanced_menu;
static menu_def_t about_menu;

bool menu_state_init(void)
{
    ESP_LOGI(TAG, "Initializing menu state machine");

    memset(&s_menu_context, 0, sizeof(s_menu_context));
    s_menu_context.current_state = MENU_STATE_KEYBOARD_MODE;
    s_menu_context.menu_active = false;
    s_menu_context.timeout_ms = 10000; // 10 seconds default timeout

    // Create all menus
    menu_create_main_menu();
    menu_create_bluetooth_menu();
    menu_create_wifi_menu();
    menu_create_led_menu();
    menu_create_advanced_menu();
    menu_create_about_menu();

    ESP_LOGI(TAG, "Menu state machine initialized");
    return true;
}

bool menu_state_process_event(input_event_t event)
{
    bool event_consumed = false;
    s_menu_context.last_activity_time = esp_timer_get_time() / 1000; // Convert to ms

    ESP_LOGD(TAG, "Processing event %d in state %d", event, s_menu_context.current_state);

    switch (s_menu_context.current_state) {
        case MENU_STATE_KEYBOARD_MODE:
            if (event == INPUT_EVENT_ENCODER_CW || event == INPUT_EVENT_ENCODER_CCW) {
                // Enter main menu on encoder rotation
                s_menu_context.current_state = MENU_STATE_MAIN_MENU;
                s_menu_context.menu_active = true;
                s_menu_context.current_selection = 0;
                menu_set_current_selection(s_menu_context.menus[MENU_STATE_MAIN_MENU], 0);
                event_consumed = true;
                ESP_LOGI(TAG, "Entered main menu");
            }
            break;

        case MENU_STATE_MAIN_MENU:
        case MENU_STATE_BLUETOOTH_MENU:
        case MENU_STATE_WIFI_MENU:
        case MENU_STATE_LED_MENU:
        case MENU_STATE_ADVANCED_MENU:
        case MENU_STATE_ABOUT_MENU:
            {
                menu_def_t *current_menu = s_menu_context.menus[s_menu_context.current_state];
                if (!current_menu) break;

                switch (event) {
                    case INPUT_EVENT_ENCODER_CW:
                        // Move to next item
                        s_menu_context.current_selection = (s_menu_context.current_selection + 1) % current_menu->item_count;
                        menu_set_current_selection(current_menu, s_menu_context.current_selection);
                        event_consumed = true;
                        break;

                    case INPUT_EVENT_ENCODER_CCW:
                        // Move to previous item
                        s_menu_context.current_selection = (s_menu_context.current_selection - 1 + current_menu->item_count) % current_menu->item_count;
                        menu_set_current_selection(current_menu, s_menu_context.current_selection);
                        event_consumed = true;
                        break;

                    case INPUT_EVENT_ENTER:
                        // Execute current item action or navigate to submenu
                        if (current_menu->current_item) {
                            if (current_menu->current_item->action_func) {
                                // Execute action function
                                current_menu->current_item->action_func();
                            } else if (current_menu->current_item->target_state != MENU_STATE_KEYBOARD_MODE) {
                                // Navigate to submenu
                                s_menu_context.previous_state = s_menu_context.current_state;
                                s_menu_context.current_state = current_menu->current_item->target_state;
                                s_menu_context.current_selection = 0;
                                menu_set_current_selection(s_menu_context.menus[s_menu_context.current_state], 0);
                            }
                        }
                        event_consumed = true;
                        break;

                    case INPUT_EVENT_ESC:
                        // Return to parent menu or keyboard mode
                        if (current_menu->parent_state == MENU_STATE_KEYBOARD_MODE) {
                            s_menu_context.current_state = MENU_STATE_KEYBOARD_MODE;
                            s_menu_context.menu_active = false;
                            ESP_LOGI(TAG, "Returned to keyboard mode");
                        } else {
                            s_menu_context.current_state = current_menu->parent_state;
                            s_menu_context.current_selection = 0;
                            menu_set_current_selection(s_menu_context.menus[s_menu_context.current_state], 0);
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
            }
            break;

        default:
            break;
    }

    return event_consumed;
}

menu_state_t menu_state_get_current(void)
{
    return s_menu_context.current_state;
}

bool menu_state_is_active(void)
{
    return s_menu_context.menu_active;
}

menu_def_t* menu_state_get_current_menu(void)
{
    if (!s_menu_context.menu_active) {
        return NULL;
    }
    return s_menu_context.menus[s_menu_context.current_state];
}

void menu_state_return_to_keyboard(void)
{
    s_menu_context.current_state = MENU_STATE_KEYBOARD_MODE;
    s_menu_context.menu_active = false;
    ESP_LOGI(TAG, "Returned to keyboard mode");
}

void menu_state_update_timeout(void)
{
    if (!s_menu_context.menu_active || s_menu_context.timeout_ms == 0) {
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
}

// Menu creation functions
static void menu_create_main_menu(void)
{
    main_menu.state = MENU_STATE_MAIN_MENU;
    main_menu.title = "Main Menu";
    main_menu.parent_state = MENU_STATE_KEYBOARD_MODE;
    main_menu.items = NULL;
    main_menu.current_item = NULL;
    main_menu.item_count = 0;

    menu_add_item(&main_menu, menu_create_item("1. Bluetooth", MENU_STATE_BLUETOOTH_MENU, NULL));
    menu_add_item(&main_menu, menu_create_item("2. WiFi", MENU_STATE_WIFI_MENU, NULL));
    menu_add_item(&main_menu, menu_create_item("3. LED", MENU_STATE_LED_MENU, NULL));
    menu_add_item(&main_menu, menu_create_item("4. Advanced", MENU_STATE_ADVANCED_MENU, NULL));
    menu_add_item(&main_menu, menu_create_item("5. About", MENU_STATE_ABOUT_MENU, NULL));

    s_menu_context.menus[MENU_STATE_MAIN_MENU] = &main_menu;
}

static void menu_create_bluetooth_menu(void)
{
    bluetooth_menu.state = MENU_STATE_BLUETOOTH_MENU;
    bluetooth_menu.title = "Bluetooth Settings";
    bluetooth_menu.parent_state = MENU_STATE_MAIN_MENU;
    bluetooth_menu.items = NULL;
    bluetooth_menu.current_item = NULL;
    bluetooth_menu.item_count = 0;

    menu_add_item(&bluetooth_menu, menu_create_item("1) Turn On/Off BT", MENU_STATE_BLUETOOTH_MENU, action_bluetooth_toggle));
    menu_add_item(&bluetooth_menu, menu_create_item("2) Keyboard Pairing", MENU_STATE_BLUETOOTH_MENU, action_bluetooth_pair_keyboard));
    menu_add_item(&bluetooth_menu, menu_create_item("3) Admin App Pairing", MENU_STATE_BLUETOOTH_MENU, action_bluetooth_pair_admin));

    s_menu_context.menus[MENU_STATE_BLUETOOTH_MENU] = &bluetooth_menu;
}

static void menu_create_wifi_menu(void)
{
    wifi_menu.state = MENU_STATE_WIFI_MENU;
    wifi_menu.title = "WiFi Settings";
    wifi_menu.parent_state = MENU_STATE_MAIN_MENU;
    wifi_menu.items = NULL;
    wifi_menu.current_item = NULL;
    wifi_menu.item_count = 0;

    menu_add_item(&wifi_menu, menu_create_item("1) Turn On/Off WiFi", MENU_STATE_WIFI_MENU, action_wifi_toggle));
    menu_add_item(&wifi_menu, menu_create_item("2) WiFi Settings", MENU_STATE_WIFI_MENU, action_wifi_settings));

    s_menu_context.menus[MENU_STATE_WIFI_MENU] = &wifi_menu;
}

static void menu_create_led_menu(void)
{
    led_menu.state = MENU_STATE_LED_MENU;
    led_menu.title = "LED Settings";
    led_menu.parent_state = MENU_STATE_MAIN_MENU;
    led_menu.items = NULL;
    led_menu.current_item = NULL;
    led_menu.item_count = 0;

    menu_add_item(&led_menu, menu_create_item("1) Turn On/Off LED", MENU_STATE_LED_MENU, action_led_toggle));
    menu_add_item(&led_menu, menu_create_item("2) Choose LED Pattern", MENU_STATE_LED_MENU, action_led_pattern));

    s_menu_context.menus[MENU_STATE_LED_MENU] = &led_menu;
}

static void menu_create_advanced_menu(void)
{
    advanced_menu.state = MENU_STATE_ADVANCED_MENU;
    advanced_menu.title = "Advanced Features";
    advanced_menu.parent_state = MENU_STATE_MAIN_MENU;
    advanced_menu.items = NULL;
    advanced_menu.current_item = NULL;
    advanced_menu.item_count = 0;

    menu_add_item(&advanced_menu, menu_create_item("1) Keyboard Lock", MENU_STATE_ADVANCED_MENU, action_keyboard_lock_toggle));
    menu_add_item(&advanced_menu, menu_create_item("2) Input Logging", MENU_STATE_ADVANCED_MENU, action_input_logging_toggle));
    menu_add_item(&advanced_menu, menu_create_item("3) Stand-Up Reminder", MENU_STATE_ADVANCED_MENU, action_standup_reminder_toggle));

    s_menu_context.menus[MENU_STATE_ADVANCED_MENU] = &advanced_menu;
}

static void menu_create_about_menu(void)
{
    about_menu.state = MENU_STATE_ABOUT_MENU;
    about_menu.title = "About Keyboard";
    about_menu.parent_state = MENU_STATE_MAIN_MENU;
    about_menu.items = NULL;
    about_menu.current_item = NULL;
    about_menu.item_count = 0;

    menu_add_item(&about_menu, menu_create_item("View Information", MENU_STATE_ABOUT_MENU, action_show_about));

    s_menu_context.menus[MENU_STATE_ABOUT_MENU] = &about_menu;
}

static menu_item_t* menu_create_item(const char *text, menu_state_t target_state, bool (*action_func)(void))
{
    static int item_index = 0;
    static menu_item_t all_items[20]; // Static storage for all menu items

    if (item_index >= 20) {
        ESP_LOGE(TAG, "Too many menu items created");
        return NULL;
    }

    menu_item_t *item = &all_items[item_index++];
    item->text = text;
    item->target_state = target_state;
    item->action_func = action_func;
    item->next = NULL;
    item->prev = NULL;

    return item;
}

static void menu_add_item(menu_def_t *menu, menu_item_t *item)
{
    if (!menu || !item) return;

    if (!menu->items) {
        // First item
        menu->items = item;
        menu->current_item = item;
        item->next = item;
        item->prev = item;
    } else {
        // Insert at end of circular list
        menu_item_t *last = menu->items->prev;
        last->next = item;
        item->prev = last;
        item->next = menu->items;
        menu->items->prev = item;
    }

    menu->item_count++;
}

static void menu_set_current_selection(menu_def_t *menu, int selection)
{
    if (!menu || !menu->items || selection < 0 || selection >= menu->item_count) {
        return;
    }

    menu_item_t *item = menu->items;
    for (int i = 0; i < selection; i++) {
        item = item->next;
    }
    menu->current_item = item;
}

// Action function implementations
bool action_bluetooth_toggle(void)
{
    ESP_LOGI(TAG, "Bluetooth toggle action");
    // TODO: Implement Bluetooth toggle functionality
    return true;
}

bool action_bluetooth_pair_keyboard(void)
{
    ESP_LOGI(TAG, "Bluetooth keyboard pairing action");
    // TODO: Implement Bluetooth keyboard pairing
    return true;
}

bool action_bluetooth_pair_admin(void)
{
    ESP_LOGI(TAG, "Bluetooth admin app pairing action");
    // TODO: Implement Bluetooth admin app pairing
    return true;
}

bool action_wifi_toggle(void)
{
    ESP_LOGI(TAG, "WiFi toggle action");
    // TODO: Implement WiFi toggle functionality
    return true;
}

bool action_wifi_settings(void)
{
    ESP_LOGI(TAG, "WiFi settings action");
    // TODO: Implement WiFi settings functionality
    return true;
}

bool action_led_toggle(void)
{
    ESP_LOGI(TAG, "LED toggle action");
    // TODO: Implement LED toggle functionality
    return true;
}

bool action_led_pattern(void)
{
    ESP_LOGI(TAG, "LED pattern selection action");
    // TODO: Implement LED pattern selection
    return true;
}

bool action_keyboard_lock_toggle(void)
{
    ESP_LOGI(TAG, "Keyboard lock toggle action");
    // TODO: Implement keyboard lock toggle
    return true;
}

bool action_input_logging_toggle(void)
{
    ESP_LOGI(TAG, "Input logging toggle action");
    // TODO: Implement input logging toggle
    return true;
}

bool action_standup_reminder_toggle(void)
{
    ESP_LOGI(TAG, "Stand-up reminder toggle action");
    // TODO: Implement stand-up reminder toggle
    return true;
}

bool action_show_about(void)
{
    ESP_LOGI(TAG, "Show about information action");
    // TODO: Implement about information display
    return true;
}
