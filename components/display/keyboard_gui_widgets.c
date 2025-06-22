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
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "lvgl.h"
#include "lcd_hardware.h"
#include "menu_state_machine.h"
#include "keyboard_gui_construct.h"

#define TAG "keyboard_gui_construct"

/**
 * @brief Keyboard statistics structure
 */
typedef struct {
    uint32_t total_char_count;
    uint32_t session_char_count;
    uint32_t session_start_time;
} keyboard_stats_t;

// GUI context structures for different menu types
typedef struct {
    lv_obj_t *container;
    lv_obj_t *stats_label;
    lv_obj_t *total_count_label;
    lv_obj_t *char_count_label;
    lv_timer_t *update_timer;  // Timer for periodic updates
} keyboard_info_gui_t;

typedef struct {
    lv_obj_t *container;
    lv_obj_t *menu_title_label;
    lv_obj_t *menu_items_list;
    lv_obj_t *current_selection_bar;
} nonleaf_item_gui_t;

static lv_obj_t *s_main_screen = NULL;
static keyboard_stats_t s_keyboard_stats = {0};

static keyboard_info_gui_t* create_keyboard_info_gui(void);
static nonleaf_item_gui_t* create_nonleaf_item_gui(void);
static void update_keyboard_info_display(keyboard_info_gui_t *gui);
static void update_nonleaf_item_display(nonleaf_item_gui_t *gui, struct menu_item *menu_item);
static void keyboard_info_timer_cb(lv_timer_t *timer);

// GUI setup and teardown functions for menu items
static esp_err_t prepare_keyboard_info_gui(struct menu_item *self);
static esp_err_t post_keyboard_info_gui(struct menu_item *self);
static esp_err_t prepare_nonleaf_item_gui(struct menu_item *self);
static esp_err_t post_nonleaf_item_gui(struct menu_item *self);

void keyboard_gui_init_keyboard_stats(void)
{    // Initialize statistics
    memset(&s_keyboard_stats, 0, sizeof(keyboard_stats_t));
    s_keyboard_stats.session_start_time = esp_timer_get_time() / 1000; // Convert to ms

    // Create main screen
    s_main_screen = lv_screen_active();
    lv_obj_set_style_bg_color(s_main_screen, lv_color_black(), 0);
}

void keyboard_gui_update_stats(uint32_t count)
{
    s_keyboard_stats.total_char_count += count;
    s_keyboard_stats.session_char_count += count;
}

void keyboard_gui_reset_session_stats(void)
{
    s_keyboard_stats.session_char_count = 0;
    s_keyboard_stats.session_start_time = esp_timer_get_time() / 1000;
    ESP_LOGI(TAG, "Session statistics reset");
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
    lv_obj_set_style_pad_all(gui->container, 20, 0);  // Increased padding from 2 to 20

    // Hide container initially - will be shown when prepare_gui_func is called
    lv_obj_add_flag(gui->container, LV_OBJ_FLAG_HIDDEN);

    // Main stats label
    gui->stats_label = lv_label_create(gui->container);
    lv_obj_set_style_text_color(gui->stats_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(gui->stats_label, &lv_font_montserrat_14, 0);  // Changed to available font
    lv_obj_align(gui->stats_label, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_label_set_text(gui->stats_label, "MK32 Keyboard");

    // Total count label
    gui->total_count_label = lv_label_create(gui->container);
    lv_obj_set_style_text_color(gui->total_count_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(gui->total_count_label, &lv_font_montserrat_12, 0);  // Changed to available font
    lv_obj_align(gui->total_count_label, LV_ALIGN_TOP_LEFT, 0, 22);  // Position first
    lv_label_set_text(gui->total_count_label, "Total: 0");

    // Char count label
    gui->char_count_label = lv_label_create(gui->container);
    lv_obj_set_style_text_color(gui->char_count_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(gui->char_count_label, &lv_font_montserrat_12, 0);  // Changed to available font
    lv_obj_align(gui->char_count_label, LV_ALIGN_TOP_LEFT, 0, 42);  // Position below total count
    lv_label_set_text(gui->char_count_label, "Chars: 0");

    // Instructions
    lv_obj_t *instruction_label = lv_label_create(gui->container);
    lv_obj_set_style_text_color(instruction_label, lv_color_hex(0x808080), 0);
    lv_obj_set_style_text_font(instruction_label, &lv_font_montserrat_10, 0);  // Increased from 8 to 10
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

    // Hide container initially - will be shown when prepare_gui_func is called
    lv_obj_add_flag(gui->container, LV_OBJ_FLAG_HIDDEN);

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

    // Update total count
    snprintf(buffer, sizeof(buffer), "Total: %lu", s_keyboard_stats.total_char_count);
    lv_label_set_text(gui->total_count_label, buffer);

    // Update character count
    snprintf(buffer, sizeof(buffer), "Chars: %lu", s_keyboard_stats.session_char_count);
    lv_label_set_text(gui->char_count_label, buffer);
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
        ESP_LOGI(TAG, "Created new keyboard info GUI");
    }

    keyboard_info_gui_t *gui = (keyboard_info_gui_t *)self->user_ctx;

    // Show the keyboard info container
    lv_obj_remove_flag(gui->container, LV_OBJ_FLAG_HIDDEN);
    ESP_LOGI(TAG, "Showed keyboard info container");

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
    ESP_LOGI(TAG, "Hidden keyboard info container");

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
        ESP_LOGI(TAG, "Created new nonleaf item GUI for: %s", self->text);
    }

    nonleaf_item_gui_t *gui = (nonleaf_item_gui_t *)self->user_ctx;

    // Show the nonleaf item container
    lv_obj_remove_flag(gui->container, LV_OBJ_FLAG_HIDDEN);
    ESP_LOGI(TAG, "Showed nonleaf item container for: %s", self->text);

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
    ESP_LOGI(TAG, "Hidden nonleaf item container for: %s", self->text);

    return ESP_OK;
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
