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

// GUI context structures for different menu types
typedef struct {
    lv_obj_t *container;
    lv_obj_t *stats_label;
    lv_obj_t *char_count_label;
    lv_obj_t *key_count_label;
    lv_obj_t *last_key_label;
    lv_timer_t *update_timer;  // Timer for periodic updates
} keyboard_info_gui_t;

typedef struct {
    lv_obj_t *container;
    lv_obj_t *menu_title_label;
    lv_obj_t *menu_items_list;
    lv_obj_t *current_selection_bar;
} nonleaf_item_gui_t;

// Global state for GUI
static bool s_gui_initialized = false;
static lv_obj_t *s_main_screen = NULL;
static keyboard_stats_t s_keyboard_stats = {0};

// Forward declarations
static keyboard_info_gui_t* create_keyboard_info_gui(void);
static nonleaf_item_gui_t* create_nonleaf_item_gui(void);
static void destroy_keyboard_info_gui(keyboard_info_gui_t *gui);
static void destroy_nonleaf_item_gui(nonleaf_item_gui_t *gui);
static void update_keyboard_info_display(keyboard_info_gui_t *gui);
static void update_nonleaf_item_display(nonleaf_item_gui_t *gui, struct menu_item *menu_item);
static void keyboard_info_timer_cb(lv_timer_t *timer);
static char keycode_to_char(uint16_t keycode);

// GUI setup and teardown functions for menu items
static esp_err_t prepare_keyboard_info_gui(struct menu_item *self);
static esp_err_t post_keyboard_info_gui(struct menu_item *self);
static esp_err_t prepare_nonleaf_item_gui(struct menu_item *self);
static esp_err_t post_nonleaf_item_gui(struct menu_item *self);

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

    // Initialize statistics
    memset(&s_keyboard_stats, 0, sizeof(keyboard_stats_t));
    s_keyboard_stats.session_start_time = esp_timer_get_time() / 1000; // Convert to ms

    // Create main screen
    s_main_screen = lv_screen_active();
    lv_obj_set_style_bg_color(s_main_screen, lv_color_black(), 0);

    s_gui_initialized = true;

    // Initialize display with keyboard information
    ESP_LOGI(TAG, "Displaying initial keyboard information");
    menu_state_return_to_keyboard();

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
    s_keyboard_stats.total_key_count++;
    s_keyboard_stats.session_key_count++;
    s_keyboard_stats.last_key_pressed = keycode;
    s_keyboard_stats.last_char_typed = keycode_to_char(keycode);

    // Count characters for statistics
    if (keycode >= 0x04 && keycode <= 0x1D) { // A-Z keys
        s_keyboard_stats.session_char_count++;
    } else if (keycode >= 0x1E && keycode <= 0x27) { // 1-0 keys
        s_keyboard_stats.session_char_count++;
    } else if (keycode == 0x2C) { // Space key
        s_keyboard_stats.session_char_count++;
    } else if (keycode == 0x28) { // Enter key
        s_keyboard_stats.session_char_count++;
    }
}

keyboard_stats_t* keyboard_gui_get_stats(void)
{
    return &s_keyboard_stats;
}

void keyboard_gui_reset_session_stats(void)
{
    s_keyboard_stats.session_key_count = 0;
    s_keyboard_stats.session_char_count = 0;
    s_keyboard_stats.session_start_time = esp_timer_get_time() / 1000;
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
    if (!s_gui_initialized) {
        return ESP_OK;
    }

    // Clean up LVGL objects
    if (s_main_screen) {
        lv_obj_clean(s_main_screen);
    }

    // Deinitialize LCD hardware
    lcd_hardware_deinit();

    s_gui_initialized = false;
    ESP_LOGI(TAG, "Keyboard GUI deinitialized");

    return ESP_OK;
}

// UI Creation Functions
static keyboard_info_gui_t* create_keyboard_info_gui(void)
{
    keyboard_info_gui_t *gui = malloc(sizeof(keyboard_info_gui_t));
    if (!gui) {
        ESP_LOGE(TAG, "Failed to allocate keyboard info GUI");
        return NULL;
    }

    // Create container for keyboard info
    gui->container = lv_obj_create(s_main_screen);
    lv_obj_set_size(gui->container, LCD_WIDTH, LCD_HEIGHT);
    lv_obj_set_pos(gui->container, 0, 0);
    lv_obj_set_style_bg_color(gui->container, lv_color_black(), 0);
    lv_obj_set_style_border_width(gui->container, 0, 0);
    lv_obj_set_style_pad_all(gui->container, 2, 0);

    // Main stats label
    gui->stats_label = lv_label_create(gui->container);
    lv_obj_set_style_text_color(gui->stats_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(gui->stats_label, &lv_font_montserrat_12, 0);
    lv_obj_align(gui->stats_label, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_label_set_text(gui->stats_label, "MK32 Keyboard");

    // Char count label
    gui->char_count_label = lv_label_create(gui->container);
    lv_obj_set_style_text_color(gui->char_count_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(gui->char_count_label, &lv_font_montserrat_10, 0);
    lv_obj_align(gui->char_count_label, LV_ALIGN_TOP_LEFT, 0, 15);
    lv_label_set_text(gui->char_count_label, "Chars: 0");

    // Key count label
    gui->key_count_label = lv_label_create(gui->container);
    lv_obj_set_style_text_color(gui->key_count_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(gui->key_count_label, &lv_font_montserrat_10, 0);
    lv_obj_align(gui->key_count_label, LV_ALIGN_TOP_LEFT, 0, 30);
    lv_label_set_text(gui->key_count_label, "Keys: 0/0");

    // Last key label
    gui->last_key_label = lv_label_create(gui->container);
    lv_obj_set_style_text_color(gui->last_key_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(gui->last_key_label, &lv_font_montserrat_10, 0);
    lv_obj_align(gui->last_key_label, LV_ALIGN_TOP_LEFT, 0, 45);
    lv_label_set_text(gui->last_key_label, "Last: -");

    // Instructions
    lv_obj_t *instruction_label = lv_label_create(gui->container);
    lv_obj_set_style_text_color(instruction_label, lv_color_hex(0x808080), 0);
    lv_obj_set_style_text_font(instruction_label, &lv_font_montserrat_8, 0);
    lv_obj_align(instruction_label, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_label_set_text(instruction_label, "Rotate encoder for menu");

    // Create timer for periodic updates (100ms interval)
    gui->update_timer = lv_timer_create(keyboard_info_timer_cb, 100, gui);
    lv_timer_pause(gui->update_timer);  // Start paused, will be enabled when GUI is shown

    return gui;
}

static void keyboard_info_timer_cb(lv_timer_t *timer)
{
    keyboard_info_gui_t *gui = (keyboard_info_gui_t *)lv_timer_get_user_data(timer);
    if (gui) {
        update_keyboard_info_display(gui);
    }
}

static nonleaf_item_gui_t* create_nonleaf_item_gui(void)
{
    nonleaf_item_gui_t *gui = malloc(sizeof(nonleaf_item_gui_t));
    if (!gui) {
        ESP_LOGE(TAG, "Failed to allocate nonleaf item GUI");
        return NULL;
    }

    // Create container for nonleaf item
    gui->container = lv_obj_create(s_main_screen);
    lv_obj_set_size(gui->container, LCD_WIDTH, LCD_HEIGHT);
    lv_obj_set_pos(gui->container, 0, 0);
    lv_obj_set_style_bg_color(gui->container, lv_color_black(), 0);
    lv_obj_set_style_border_width(gui->container, 0, 0);
    lv_obj_set_style_pad_all(gui->container, 2, 0);

    // Menu title
    gui->menu_title_label = lv_label_create(gui->container);
    lv_obj_set_style_text_color(gui->menu_title_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(gui->menu_title_label, &lv_font_montserrat_12, 0);
    lv_obj_align(gui->menu_title_label, LV_ALIGN_TOP_MID, 0, 0);
    lv_label_set_text(gui->menu_title_label, "Menu");

    // Menu items container
    gui->menu_items_list = lv_obj_create(gui->container);
    lv_obj_set_size(gui->menu_items_list, LCD_WIDTH - 4, LCD_HEIGHT - 30);
    lv_obj_set_pos(gui->menu_items_list, 2, 15);
    lv_obj_set_style_bg_color(gui->menu_items_list, lv_color_black(), 0);
    lv_obj_set_style_border_width(gui->menu_items_list, 0, 0);
    lv_obj_set_style_pad_all(gui->menu_items_list, 2, 0);

    // Selection highlight bar
    gui->current_selection_bar = lv_obj_create(gui->menu_items_list);
    lv_obj_set_size(gui->current_selection_bar, LCD_WIDTH - 8, 12);
    lv_obj_set_style_bg_color(gui->current_selection_bar, lv_color_hex(0x333333), 0);
    lv_obj_set_style_border_width(gui->current_selection_bar, 1, 0);
    lv_obj_set_style_border_color(gui->current_selection_bar, lv_color_white(), 0);

    // Instructions
    lv_obj_t *instruction_label = lv_label_create(gui->container);
    lv_obj_set_style_text_color(instruction_label, lv_color_hex(0x808080), 0);
    lv_obj_set_style_text_font(instruction_label, &lv_font_montserrat_8, 0);
    lv_obj_align(instruction_label, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_label_set_text(instruction_label, "Enter=Select ESC=Back");

    return gui;
}

static void update_keyboard_info_display(keyboard_info_gui_t *gui)
{
    if (!gui) {
        return;
    }

    char buffer[64];

    // Update character count
    snprintf(buffer, sizeof(buffer), "Chars: %lu", s_keyboard_stats.session_char_count);
    lv_label_set_text(gui->char_count_label, buffer);

    // Update key counts
    snprintf(buffer, sizeof(buffer), "Keys: %lu/%lu",
             s_keyboard_stats.session_key_count,
             s_keyboard_stats.total_key_count);
    lv_label_set_text(gui->key_count_label, buffer);

    // Update last key
    if (s_keyboard_stats.last_char_typed != 0) {
        snprintf(buffer, sizeof(buffer), "Last: %c (0x%02X)",
                 s_keyboard_stats.last_char_typed,
                 s_keyboard_stats.last_key_pressed);
    } else {
        snprintf(buffer, sizeof(buffer), "Last: 0x%02X", s_keyboard_stats.last_key_pressed);
    }
    lv_label_set_text(gui->last_key_label, buffer);
}

static void update_nonleaf_item_display(nonleaf_item_gui_t *gui, struct menu_item *menu_item)
{
    if (!gui || !menu_item) {
        return;
    }

    // Update menu title
    lv_label_set_text(gui->menu_title_label, menu_item->text);

    // Clear existing menu items
    lv_obj_clean(gui->menu_items_list);

    // Recreate selection bar
    gui->current_selection_bar = lv_obj_create(gui->menu_items_list);
    lv_obj_set_size(gui->current_selection_bar, LCD_WIDTH - 8, 12);
    lv_obj_set_style_bg_color(gui->current_selection_bar, lv_color_hex(0x333333), 0);
    lv_obj_set_style_border_width(gui->current_selection_bar, 1, 0);
    lv_obj_set_style_border_color(gui->current_selection_bar, lv_color_white(), 0);

    // Add menu items (children of current menu)
    struct menu_item *child;
    int y_pos = 0;
    int item_index = 0;

    LIST_FOREACH(child, &menu_item->children, entry) {
        if (y_pos >= LCD_HEIGHT - 35) {
            break; // Don't overflow the display
        }

        lv_obj_t *item_label = lv_label_create(gui->menu_items_list);
        lv_obj_set_style_text_color(item_label, lv_color_white(), 0);
        lv_obj_set_style_text_font(item_label, &lv_font_montserrat_10, 0);
        lv_obj_set_pos(item_label, 2, y_pos);
        lv_label_set_text(item_label, child->text);

        // Highlight current selection
        if (child == menu_item->focused_child) {
            lv_obj_set_pos(gui->current_selection_bar, 0, y_pos - 1);
        }

        y_pos += 12;
        item_index++;
    }

    // If no children, show that this is a leaf menu
    if (LIST_EMPTY(&menu_item->children)) {
        lv_obj_t *info_label = lv_label_create(gui->menu_items_list);
        lv_obj_set_style_text_color(info_label, lv_color_hex(0x808080), 0);
        lv_obj_set_style_text_font(info_label, &lv_font_montserrat_10, 0);
        lv_obj_set_pos(info_label, 2, 0);
        lv_label_set_text(info_label, "Press ENTER to execute");

        // Hide selection bar for leaf menus
        lv_obj_add_flag(gui->current_selection_bar, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_remove_flag(gui->current_selection_bar, LV_OBJ_FLAG_HIDDEN);
    }
}

// GUI setup and teardown functions for menu items
static esp_err_t prepare_keyboard_info_gui(struct menu_item *self)
{
    ESP_LOGI(TAG, "Preparing keyboard info GUI");

    if (!self) {
        return ESP_ERR_INVALID_ARG;
    }

    // Create keyboard GUI if not already created
    if (!self->user_ctx) {
        self->user_ctx = create_keyboard_info_gui();
        if (!self->user_ctx) {
            ESP_LOGE(TAG, "Failed to create keyboard info GUI");
            return ESP_ERR_NO_MEM;
        }
    }

    keyboard_info_gui_t *gui = (keyboard_info_gui_t *)self->user_ctx;

    // Show the keyboard info container
    lv_obj_remove_flag(gui->container, LV_OBJ_FLAG_HIDDEN);

    // Start periodic updates
    if (gui->update_timer) {
        lv_timer_resume(gui->update_timer);
    }

    return ESP_OK;
}

static esp_err_t post_keyboard_info_gui(struct menu_item *self)
{
    ESP_LOGD(TAG, "Post keyboard info GUI cleanup");

    if (!self || !self->user_ctx) {
        return ESP_OK;
    }

    keyboard_info_gui_t *gui = (keyboard_info_gui_t *)self->user_ctx;

    // Stop periodic updates
    if (gui->update_timer) {
        lv_timer_pause(gui->update_timer);
    }

    // Hide the keyboard info container
    lv_obj_add_flag(gui->container, LV_OBJ_FLAG_HIDDEN);

    return ESP_OK;
}

static esp_err_t prepare_nonleaf_item_gui(struct menu_item *self)
{
    ESP_LOGI(TAG, "Preparing nonleaf item GUI for: %s", self ? self->text : "NULL");

    if (!self) {
        return ESP_ERR_INVALID_ARG;
    }

    // Create menu GUI if not already created
    if (!self->user_ctx) {
        self->user_ctx = create_nonleaf_item_gui();
        if (!self->user_ctx) {
            ESP_LOGE(TAG, "Failed to create nonleaf item GUI");
            return ESP_ERR_NO_MEM;
        }
    }

    nonleaf_item_gui_t *gui = (nonleaf_item_gui_t *)self->user_ctx;

    // Show the nonleaf item container
    lv_obj_remove_flag(gui->container, LV_OBJ_FLAG_HIDDEN);

    // Update menu display to reflect current state
    update_nonleaf_item_display(gui, self);

    return ESP_OK;
}

static esp_err_t post_nonleaf_item_gui(struct menu_item *self)
{
    ESP_LOGD(TAG, "Post nonleaf item GUI cleanup for: %s", self ? self->text : "NULL");

    if (!self || !self->user_ctx) {
        return ESP_OK;
    }

    nonleaf_item_gui_t *gui = (nonleaf_item_gui_t *)self->user_ctx;

    // Hide the nonleaf item container
    lv_obj_add_flag(gui->container, LV_OBJ_FLAG_HIDDEN);

    return ESP_OK;
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

static void destroy_keyboard_info_gui(keyboard_info_gui_t *gui)
{
    if (!gui) {
        return;
    }

    // Clean up timer
    if (gui->update_timer) {
        lv_timer_del(gui->update_timer);
    }

    if (gui->container) {
        lv_obj_del(gui->container);
    }

    free(gui);
}

static void destroy_nonleaf_item_gui(nonleaf_item_gui_t *gui)
{
    if (!gui) {
        return;
    }

    if (gui->container) {
        lv_obj_del(gui->container);
    }

    free(gui);
}

// Public API functions for menu integration
esp_err_t keyboard_gui_prepare_keyboard_info(struct menu_item *self)
{
    return prepare_keyboard_info_gui(self);
}

esp_err_t keyboard_gui_post_keyboard_info(struct menu_item *self)
{
    return post_keyboard_info_gui(self);
}

esp_err_t keyboard_gui_prepare_nonleaf_item(struct menu_item *self)
{
    return prepare_nonleaf_item_gui(self);
}

esp_err_t keyboard_gui_post_nonleaf_item(struct menu_item *self)
{
    return post_nonleaf_item_gui(self);
}
