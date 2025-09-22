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

#include "esp_log.h"
#include "esp_timer.h"
#include "esp_check.h"
#include "esp_gap_ble_api.h"
#include <string.h>
#include "lvgl.h"
#include <stdio.h>
#include "drv_loop.h"
#include "key_definitions.h"
#include "action_code.h"
#include "keyboard_gui.h"
#include "lcd_hardware.h"
#include "menu_state_machine.h"
#include "keyboard_gui_widgets.h"

#define UPDATE_LVGL_PERIOD_MS  250  // Update LVGL every second

static const char *TAG = "keyboard_gui";

typedef struct {
    int8_t input_event;
    char keycode;  // Keycode of the pressed key
} input_event_data_t;

typedef struct {
    struct menu_item *menu_item;  // Menu item associated with the event
    enum passkey_event_e input_event;    // Passkey event type
    esp_bd_addr_t bd_addr;       // Hardware address of the device being paired
} passkey_event_data_t;

enum {
    UPDATE_LVGL_EVENT = 0,  // Update LVGL display
    GUI_INPUT_EVENT,
    PROMPT_FOR_PASSKEY_EVENT,
};

ESP_EVENT_DEFINE_BASE(KEYBOARD_GUI_EVENTS);

// Global state for GUI
static bool s_gui_initialized = false;

static void update_lvgl_timer_func(void *arg)
{
    esp_err_t ret = drv_loop_post_event(
        KEYBOARD_GUI_EVENTS,
        UPDATE_LVGL_EVENT,
        NULL,
        0,
        0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to post LVGL update event: %s", esp_err_to_name(ret));
    }
}

static void update_lvgl_event_handler(void *arg, esp_event_base_t base, int32_t id, void *event_data)
{
    if (base == KEYBOARD_GUI_EVENTS && id == UPDATE_LVGL_EVENT) {
        keyboard_gui_update();
    }
}

static void gui_input_event_handler(void *arg, esp_event_base_t base, int32_t id, void *event_data)
{
    if (base == KEYBOARD_GUI_EVENTS && id == GUI_INPUT_EVENT) {
        input_event_data_t* input_evt_data = ((input_event_data_t *)event_data);
        menu_state_process_event(input_evt_data->input_event, input_evt_data->keycode);
    }
}

static void prompt_for_passkey_event_handler(void *arg, esp_event_base_t base, int32_t id, void *event_data)
{
    if (base == KEYBOARD_GUI_EVENTS && id == PROMPT_FOR_PASSKEY_EVENT) {
        passkey_event_data_t* passkey_evt_data = (passkey_event_data_t *)event_data;
        keyboard_gui_bt_pair_kb_prompt_for_passkey(passkey_evt_data->menu_item, passkey_evt_data->input_event, passkey_evt_data->bd_addr);
    }
}

static esp_err_t start_gui_routine_task(void)
{
    ESP_RETURN_ON_ERROR(drv_loop_register_handler(KEYBOARD_GUI_EVENTS, UPDATE_LVGL_EVENT, update_lvgl_event_handler, NULL), TAG, "Failed to register LVGL update event handler");

    ESP_RETURN_ON_ERROR(drv_loop_register_handler(KEYBOARD_GUI_EVENTS, GUI_INPUT_EVENT, gui_input_event_handler, NULL), TAG, "Failed to register gui input event handler");

    ESP_RETURN_ON_ERROR(drv_loop_register_handler(KEYBOARD_GUI_EVENTS, PROMPT_FOR_PASSKEY_EVENT, prompt_for_passkey_event_handler, NULL), TAG, "Failed to register prompt for passkey event handler");

    const esp_timer_create_args_t update_timer_args = {
        .callback = &update_lvgl_timer_func,
        .name = "update_lvgl_timer"
    };

    esp_timer_handle_t update_timer;
    ESP_RETURN_ON_ERROR(esp_timer_create(&update_timer_args, &update_timer), TAG, "Failed to create LVGL update timer");
    ESP_RETURN_ON_ERROR(esp_timer_start_periodic(update_timer, UPDATE_LVGL_PERIOD_MS * 1000), TAG, "Failed to start LVGL update timer");

    return ESP_OK;
}

esp_err_t keyboard_gui_init(void)
{
    if (s_gui_initialized) {
        ESP_LOGW(TAG, "Keyboard GUI already initialized");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Initializing keyboard GUI");

    // Initialize LCD hardware
    ESP_RETURN_ON_ERROR(lcd_hardware_init(), TAG, "LCD hardware init failed");

    // Initialize menu state machine
    if (!menu_state_init()) {
        ESP_LOGE(TAG, "Menu state machine init failed");
        return ESP_FAIL;
    }

    keyboard_gui_init_keyboard_stats();

    ESP_RETURN_ON_ERROR(start_gui_routine_task(), TAG, "Failed to start periodic LVGL update task");

    s_gui_initialized = true;

    // Initialize display with keyboard information
    ESP_LOGI(TAG, "Displaying initial keyboard information");
    menu_return_to_keyboard_mode();

    ESP_LOGI(TAG, "Keyboard GUI initialized successfully");
    return ESP_OK;
}

void keyboard_gui_update(void)
{
    if (!s_gui_initialized) {
        return;
    }

    // Update menu timeout
    menu_state_update_timeout();

    // The menu state machine handles all state transitions via prepare_gui_func and post_gui_func
    // No special handling needed here - all menu items manage themselves

    // Handle LVGL tasks
    lv_timer_handler();
}

// Utility: handle a single keycode for GUI input
static void keyboard_gui_handle_single_keycode(uint8_t mods, uint8_t keycode) {
    input_event_e input_event = scancode_to_input_event(keycode);
    if (input_event != INPUT_EVENT_KEYCODE) {
        keyboard_gui_post_input_event_isr(input_event, 0);
    } else {
        char printable = scancode_to_printable_char(mods & MOD_LSFT, keycode);
        if (printable != '\0') {
            keyboard_gui_post_input_event_isr(INPUT_EVENT_KEYCODE, printable);
        }
    }
}

bool keyboard_gui_handle_key_input(uint8_t mods, uint8_t *scan_code, int code_len, bool nkro_bits)
{
    if (!s_gui_initialized) {
        ESP_LOGW(TAG, "Keyboard GUI not initialized");
        return false;
    }

    bool accept_keys = menu_state_accept_keys();
    uint32_t keycode_count = 0;
    static uint8_t scan_code_state[16];

    if (!nkro_bits) {
        bool all_zero = true;
        // Standard array of scan codes
        for (int i = 0; i < code_len; i++)
        {
            if (scan_code[i] == 0)
                continue;

            all_zero = false;
            if ((scan_code_state[scan_code[i] >> 3] & (1 << (scan_code[i] & 0x7))) == 0)
            {
                if (accept_keys)
                {
                    keyboard_gui_handle_single_keycode(mods, scan_code[i]);
                }
                else
                {
                    input_event_e input_event = scancode_to_input_event(scan_code[i]);
                    if (input_event == INPUT_EVENT_KEYCODE)
                    {
                        keycode_count++;
                    }
                }
            }
        }

        memset(scan_code_state, 0, sizeof(scan_code_state));
        if (!all_zero) {
            for (int i = 0; i < code_len; i++)
            {
                scan_code_state[scan_code[i] >> 3] |= (1 << (scan_code[i] & 0x7));
            }
        }
    } else {
        // NKRO bit array: each bit is a keycode (0x00-0x7F)
        for (int byte_idx = 0; byte_idx < code_len; byte_idx++) {
            uint8_t byte = scan_code[byte_idx];
            if (byte == 0) {
                scan_code_state[byte_idx] = 0;
                continue;
            }

            for (int bit_idx = 0; bit_idx < 8; bit_idx++) {
                if ((byte & (1 << bit_idx)) == 0) {
                    scan_code_state[byte_idx] &= ~(1 << bit_idx);
                    continue;
                }
                uint8_t keycode = (byte_idx << 3) + bit_idx;

                if ((scan_code_state[byte_idx] & (1 << bit_idx)) == 0)
                {
                    if (accept_keys)
                    {
                        keyboard_gui_handle_single_keycode(mods, keycode);
                    }
                    else
                    {
                        input_event_e input_event = scancode_to_input_event(keycode);
                        if (input_event == INPUT_EVENT_KEYCODE)
                        {
                            keycode_count++;
                        }
                    }

                    scan_code_state[byte_idx] |= (1 << bit_idx);
                }
            }
        }
    }

    if (accept_keys) {
        return true;
    } else {
        keyboard_gui_update_stats(keycode_count);
        return false;
    }
}

esp_err_t keyboard_gui_post_input_event_isr(input_event_e event, char keycode)
{
    input_event_data_t input_evt_data = {
        .input_event = event,
        .keycode = keycode
    };

    return drv_loop_post_event_isr(KEYBOARD_GUI_EVENTS, GUI_INPUT_EVENT, &input_evt_data, sizeof(input_evt_data));
}

esp_err_t keyboard_gui_post_prompt_for_passkey_event(struct menu_item *bt_pair_pc,
                                                     enum passkey_event_e event,
                                                     esp_bd_addr_t bd_addr)
{
    passkey_event_data_t passkey_evt_data = {
        .menu_item = bt_pair_pc,
        .input_event = event
    };

    memcpy(passkey_evt_data.bd_addr, bd_addr, sizeof(esp_bd_addr_t));

    return drv_loop_post_event_isr(KEYBOARD_GUI_EVENTS, PROMPT_FOR_PASSKEY_EVENT, &passkey_evt_data, sizeof(passkey_event_data_t));
}

esp_err_t keyboard_gui_set_brightness(uint8_t brightness)
{
    return lcd_hardware_set_backlight(brightness);
}

esp_err_t keyboard_gui_display_on_off(bool on)
{
    return lcd_hardware_display_on_off(on);
}

