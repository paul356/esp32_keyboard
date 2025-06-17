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

#include "keyboard_gui.h"
#include "lcd_hardware.h"
#include "menu_state_machine.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_check.h"
#include "lvgl.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "keyboard_gui";

// GUI context
typedef struct {
    bool initialized;
    lv_obj_t *main_screen;
    lv_obj_t *keyboard_mode_container;
    lv_obj_t *menu_mode_container;
    
    // Keyboard mode widgets
    lv_obj_t *stats_label;
    lv_obj_t *wpm_label;
    lv_obj_t *key_count_label;
    lv_obj_t *last_key_label;
    
    // Menu mode widgets
    lv_obj_t *menu_title_label;
    lv_obj_t *menu_items_list;
    lv_obj_t *current_selection_bar;
    
    keyboard_stats_t stats;
    uint32_t last_update_time;
    uint32_t last_key_time;
    uint32_t char_count_for_wpm;
} gui_context_t;

static gui_context_t s_gui_context = {0};

// Forward declarations
static void create_keyboard_mode_ui(void);
static void create_menu_mode_ui(void);
static void update_keyboard_mode_display(void);
static void update_menu_mode_display(void);
static void show_keyboard_mode(void);
static void show_menu_mode(void);
static uint32_t calculate_wpm(void);
static char keycode_to_char(uint16_t keycode);

esp_err_t keyboard_gui_init(void)
{
    if (s_gui_context.initialized) {
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

    // Initialize statistics
    memset(&s_gui_context.stats, 0, sizeof(keyboard_stats_t));
    s_gui_context.stats.session_start_time = esp_timer_get_time() / 1000; // Convert to ms

    // Create main screen
    s_gui_context.main_screen = lv_screen_active();
    lv_obj_set_style_bg_color(s_gui_context.main_screen, lv_color_black(), 0);

    // Create UI components
    create_keyboard_mode_ui();
    create_menu_mode_ui();

    // Start in keyboard mode
    show_keyboard_mode();

    s_gui_context.initialized = true;
    s_gui_context.last_update_time = esp_timer_get_time() / 1000;

    ESP_LOGI(TAG, "Keyboard GUI initialized successfully");
    return ESP_OK;
}

void keyboard_gui_update(void)
{
    if (!s_gui_context.initialized) {
        return;
    }

    uint32_t current_time = esp_timer_get_time() / 1000;
    
    // Update menu timeout
    menu_state_update_timeout();
    
    // Check if we need to switch between keyboard and menu mode
    bool menu_active = menu_state_is_active();
    bool keyboard_container_visible = !lv_obj_has_flag(s_gui_context.keyboard_mode_container, LV_OBJ_FLAG_HIDDEN);
    
    if (menu_active && keyboard_container_visible) {
        show_menu_mode();
    } else if (!menu_active && !keyboard_container_visible) {
        show_keyboard_mode();
    }

    // Update display content based on current mode
    if (menu_active) {
        update_menu_mode_display();
    } else {
        // Update keyboard mode display every 100ms
        if (current_time - s_gui_context.last_update_time > 100) {
            update_keyboard_mode_display();
            s_gui_context.last_update_time = current_time;
        }
    }

    // Handle LVGL tasks
    lv_timer_handler();
}

void keyboard_gui_handle_encoder(int direction)
{
    input_event_t event = (direction > 0) ? INPUT_EVENT_ENCODER_CW : INPUT_EVENT_ENCODER_CCW;
    menu_state_process_event(event);
}

void keyboard_gui_handle_enter(void)
{
    menu_state_process_event(INPUT_EVENT_ENTER);
}

void keyboard_gui_handle_esc(void)
{
    menu_state_process_event(INPUT_EVENT_ESC);
}

void keyboard_gui_update_stats(uint16_t keycode)
{
    s_gui_context.stats.total_key_count++;
    s_gui_context.stats.session_key_count++;
    s_gui_context.stats.last_key_pressed = keycode;
    s_gui_context.stats.last_char_typed = keycode_to_char(keycode);
    s_gui_context.last_key_time = esp_timer_get_time() / 1000;
    
    // Count characters for WPM calculation (approximate)
    if (keycode >= 0x04 && keycode <= 0x1D) { // A-Z keys
        s_gui_context.char_count_for_wpm++;
    } else if (keycode == 0x2C) { // Space key
        s_gui_context.char_count_for_wpm++;
    }
    
    s_gui_context.stats.words_per_minute = calculate_wpm();
}

keyboard_stats_t* keyboard_gui_get_stats(void)
{
    return &s_gui_context.stats;
}

void keyboard_gui_reset_session_stats(void)
{
    s_gui_context.stats.session_key_count = 0;
    s_gui_context.stats.words_per_minute = 0;
    s_gui_context.stats.session_start_time = esp_timer_get_time() / 1000;
    s_gui_context.char_count_for_wpm = 0;
    ESP_LOGI(TAG, "Session statistics reset");
}

esp_err_t keyboard_gui_set_brightness(uint8_t brightness)
{
    return lcd_hardware_set_backlight(brightness);
}

esp_err_t keyboard_gui_display_on_off(bool on)
{
    return lcd_hardware_display_on_off(on);
}

esp_err_t keyboard_gui_deinit(void)
{
    if (!s_gui_context.initialized) {
        return ESP_OK;
    }

    // Clean up LVGL objects
    if (s_gui_context.main_screen) {
        lv_obj_clean(s_gui_context.main_screen);
    }

    // Deinitialize LCD hardware
    lcd_hardware_deinit();

    s_gui_context.initialized = false;
    ESP_LOGI(TAG, "Keyboard GUI deinitialized");

    return ESP_OK;
}

// UI Creation Functions
static void create_keyboard_mode_ui(void)
{
    // Create container for keyboard mode
    s_gui_context.keyboard_mode_container = lv_obj_create(s_gui_context.main_screen);
    lv_obj_set_size(s_gui_context.keyboard_mode_container, LCD_WIDTH, LCD_HEIGHT);
    lv_obj_set_pos(s_gui_context.keyboard_mode_container, 0, 0);
    lv_obj_set_style_bg_color(s_gui_context.keyboard_mode_container, lv_color_black(), 0);
    lv_obj_set_style_border_width(s_gui_context.keyboard_mode_container, 0, 0);
    lv_obj_set_style_pad_all(s_gui_context.keyboard_mode_container, 2, 0);

    // Main stats label
    s_gui_context.stats_label = lv_label_create(s_gui_context.keyboard_mode_container);
    lv_obj_set_style_text_color(s_gui_context.stats_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(s_gui_context.stats_label, &lv_font_montserrat_12, 0);
    lv_obj_align(s_gui_context.stats_label, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_label_set_text(s_gui_context.stats_label, "MK32 Keyboard");

    // WPM label
    s_gui_context.wpm_label = lv_label_create(s_gui_context.keyboard_mode_container);
    lv_obj_set_style_text_color(s_gui_context.wpm_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(s_gui_context.wpm_label, &lv_font_montserrat_10, 0);
    lv_obj_align(s_gui_context.wpm_label, LV_ALIGN_TOP_LEFT, 0, 15);
    lv_label_set_text(s_gui_context.wpm_label, "WPM: 0");

    // Key count label
    s_gui_context.key_count_label = lv_label_create(s_gui_context.keyboard_mode_container);
    lv_obj_set_style_text_color(s_gui_context.key_count_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(s_gui_context.key_count_label, &lv_font_montserrat_10, 0);
    lv_obj_align(s_gui_context.key_count_label, LV_ALIGN_TOP_LEFT, 0, 30);
    lv_label_set_text(s_gui_context.key_count_label, "Keys: 0/0");

    // Last key label
    s_gui_context.last_key_label = lv_label_create(s_gui_context.keyboard_mode_container);
    lv_obj_set_style_text_color(s_gui_context.last_key_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(s_gui_context.last_key_label, &lv_font_montserrat_10, 0);
    lv_obj_align(s_gui_context.last_key_label, LV_ALIGN_TOP_LEFT, 0, 45);
    lv_label_set_text(s_gui_context.last_key_label, "Last: -");

    // Instructions
    lv_obj_t *instruction_label = lv_label_create(s_gui_context.keyboard_mode_container);
    lv_obj_set_style_text_color(instruction_label, lv_color_hex(0x808080), 0);
    lv_obj_set_style_text_font(instruction_label, &lv_font_montserrat_8, 0);
    lv_obj_align(instruction_label, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_label_set_text(instruction_label, "Rotate encoder for menu");
}

static void create_menu_mode_ui(void)
{
    // Create container for menu mode
    s_gui_context.menu_mode_container = lv_obj_create(s_gui_context.main_screen);
    lv_obj_set_size(s_gui_context.menu_mode_container, LCD_WIDTH, LCD_HEIGHT);
    lv_obj_set_pos(s_gui_context.menu_mode_container, 0, 0);
    lv_obj_set_style_bg_color(s_gui_context.menu_mode_container, lv_color_black(), 0);
    lv_obj_set_style_border_width(s_gui_context.menu_mode_container, 0, 0);
    lv_obj_set_style_pad_all(s_gui_context.menu_mode_container, 2, 0);

    // Menu title
    s_gui_context.menu_title_label = lv_label_create(s_gui_context.menu_mode_container);
    lv_obj_set_style_text_color(s_gui_context.menu_title_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(s_gui_context.menu_title_label, &lv_font_montserrat_12, 0);
    lv_obj_align(s_gui_context.menu_title_label, LV_ALIGN_TOP_MID, 0, 0);
    lv_label_set_text(s_gui_context.menu_title_label, "Menu");

    // Menu items container
    s_gui_context.menu_items_list = lv_obj_create(s_gui_context.menu_mode_container);
    lv_obj_set_size(s_gui_context.menu_items_list, LCD_WIDTH - 4, LCD_HEIGHT - 30);
    lv_obj_set_pos(s_gui_context.menu_items_list, 2, 15);
    lv_obj_set_style_bg_color(s_gui_context.menu_items_list, lv_color_black(), 0);
    lv_obj_set_style_border_width(s_gui_context.menu_items_list, 0, 0);
    lv_obj_set_style_pad_all(s_gui_context.menu_items_list, 2, 0);

    // Selection highlight bar
    s_gui_context.current_selection_bar = lv_obj_create(s_gui_context.menu_items_list);
    lv_obj_set_size(s_gui_context.current_selection_bar, LCD_WIDTH - 8, 12);
    lv_obj_set_style_bg_color(s_gui_context.current_selection_bar, lv_color_hex(0x333333), 0);
    lv_obj_set_style_border_width(s_gui_context.current_selection_bar, 1, 0);
    lv_obj_set_style_border_color(s_gui_context.current_selection_bar, lv_color_white(), 0);

    // Instructions
    lv_obj_t *instruction_label = lv_label_create(s_gui_context.menu_mode_container);
    lv_obj_set_style_text_color(instruction_label, lv_color_hex(0x808080), 0);
    lv_obj_set_style_text_font(instruction_label, &lv_font_montserrat_8, 0);
    lv_obj_align(instruction_label, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_label_set_text(instruction_label, "Enter=Select ESC=Back");

    // Initially hide menu mode
    lv_obj_add_flag(s_gui_context.menu_mode_container, LV_OBJ_FLAG_HIDDEN);
}

static void update_keyboard_mode_display(void)
{
    char buffer[64];

    // Update WPM
    snprintf(buffer, sizeof(buffer), "WPM: %lu", s_gui_context.stats.words_per_minute);
    lv_label_set_text(s_gui_context.wpm_label, buffer);

    // Update key counts
    snprintf(buffer, sizeof(buffer), "Keys: %lu/%lu", 
             s_gui_context.stats.session_key_count, 
             s_gui_context.stats.total_key_count);
    lv_label_set_text(s_gui_context.key_count_label, buffer);

    // Update last key
    if (s_gui_context.stats.last_char_typed != 0) {
        snprintf(buffer, sizeof(buffer), "Last: %c (0x%02X)", 
                 s_gui_context.stats.last_char_typed, 
                 s_gui_context.stats.last_key_pressed);
    } else {
        snprintf(buffer, sizeof(buffer), "Last: 0x%02X", s_gui_context.stats.last_key_pressed);
    }
    lv_label_set_text(s_gui_context.last_key_label, buffer);
}

static void update_menu_mode_display(void)
{
    menu_def_t *current_menu = menu_state_get_current_menu();
    if (!current_menu) {
        return;
    }

    // Update menu title
    lv_label_set_text(s_gui_context.menu_title_label, current_menu->title);

    // Clear existing menu items
    lv_obj_clean(s_gui_context.menu_items_list);

    // Recreate selection bar
    s_gui_context.current_selection_bar = lv_obj_create(s_gui_context.menu_items_list);
    lv_obj_set_size(s_gui_context.current_selection_bar, LCD_WIDTH - 8, 12);
    lv_obj_set_style_bg_color(s_gui_context.current_selection_bar, lv_color_hex(0x333333), 0);
    lv_obj_set_style_border_width(s_gui_context.current_selection_bar, 1, 0);
    lv_obj_set_style_border_color(s_gui_context.current_selection_bar, lv_color_white(), 0);

    // Add menu items
    menu_item_t *item = current_menu->items;
    int y_pos = 0;
    int item_index = 0;
    
    if (item) {
        do {
            lv_obj_t *item_label = lv_label_create(s_gui_context.menu_items_list);
            lv_obj_set_style_text_color(item_label, lv_color_white(), 0);
            lv_obj_set_style_text_font(item_label, &lv_font_montserrat_10, 0);
            lv_obj_set_pos(item_label, 2, y_pos);
            lv_label_set_text(item_label, item->text);

            // Highlight current selection
            if (item == current_menu->current_item) {
                lv_obj_set_pos(s_gui_context.current_selection_bar, 0, y_pos - 1);
            }

            y_pos += 12;
            item = item->next;
            item_index++;
        } while (item != current_menu->items && y_pos < LCD_HEIGHT - 35);
    }
}

static void show_keyboard_mode(void)
{
    lv_obj_remove_flag(s_gui_context.keyboard_mode_container, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_gui_context.menu_mode_container, LV_OBJ_FLAG_HIDDEN);
}

static void show_menu_mode(void)
{
    lv_obj_add_flag(s_gui_context.keyboard_mode_container, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(s_gui_context.menu_mode_container, LV_OBJ_FLAG_HIDDEN);
    update_menu_mode_display();
}

static uint32_t calculate_wpm(void)
{
    uint32_t current_time = esp_timer_get_time() / 1000;
    uint32_t session_duration = current_time - s_gui_context.stats.session_start_time;
    
    if (session_duration < 1000) { // Less than 1 second
        return 0;
    }
    
    // WPM = (characters typed / 5) / (time in minutes)
    // Assuming average word length is 5 characters
    uint32_t words = s_gui_context.char_count_for_wpm / 5;
    uint32_t minutes = session_duration / 60000; // Convert ms to minutes
    
    if (minutes == 0) {
        // For less than a minute, calculate based on seconds and scale up
        uint32_t seconds = session_duration / 1000;
        return (words * 60) / seconds;
    }
    
    return words / minutes;
}

static char keycode_to_char(uint16_t keycode)
{
    // Basic USB HID keycode to ASCII conversion
    if (keycode >= 0x04 && keycode <= 0x1D) { // A-Z
        return 'A' + (keycode - 0x04);
    } else if (keycode >= 0x1E && keycode <= 0x27) { // 1-0
        if (keycode == 0x27) return '0';
        return '1' + (keycode - 0x1E);
    } else if (keycode == 0x2C) { // Space
        return ' ';
    } else if (keycode == 0x28) { // Enter
        return '\n';
    }
    return 0; // Non-printable
}
