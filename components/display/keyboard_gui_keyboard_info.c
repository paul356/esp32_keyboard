/*
 *  This program is a keyboard firmware for ESP family boards inspired by
 *  MK32 and qmk_firmware.
 *
 *  Copyright (C) 2025 github.com/paul356
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
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "lvgl.h"
#include "lcd_hardware.h"
#include "menu_state_machine.h"
#include "keyboard_gui_widgets.h"
#include "menu_icons.h"
#include "drv_loop.h"
#include "nvs_io.h"
#include "nvs.h"

#define TAG "gui_keyboard_info"

// NVS storage constants
#define NVS_NAMESPACE           "display"
#define NVS_TOTAL_COUNT_KEY     "total_key_count"
#define SAVE_TOTAL_COUNT_PERIOD_MS  30000  // Save every 30 seconds

// Event definitions for NVS save operations
enum {
    SAVE_TOTAL_COUNT_EVENT = 0,
};

ESP_EVENT_DEFINE_BASE(KEYBOARD_STATS_EVENTS);

/**
 * @brief Keyboard statistics structure
 */
typedef struct {
    uint32_t total_char_count;
    uint32_t session_char_count;
    uint32_t session_start_time;
} keyboard_stats_t;

// GUI context structure for keyboard info
typedef struct {
    lv_obj_t *container;
    lv_obj_t *total_count_icon;     // Icon for total char count
    lv_obj_t *total_count_label;
    lv_obj_t *session_count_icon;   // Icon for session char count
    lv_obj_t *char_count_label;
    lv_timer_t *update_timer;  // Timer for periodic updates
} keyboard_info_gui_t;

static keyboard_stats_t s_keyboard_stats = {0};
static esp_timer_handle_t s_save_total_count_timer = NULL;

// Static function declarations
static keyboard_info_gui_t* create_keyboard_info_gui(void);
static void update_keyboard_info_display(keyboard_info_gui_t *gui);
static void keyboard_info_timer_cb(lv_timer_t *timer);
static void save_total_count_timer_func(void *arg);
static void save_total_count_event_handler(void *arg, esp_event_base_t base, int32_t id, void *event_data);
static esp_err_t load_total_count_from_nvs(void);
static esp_err_t save_total_count_to_nvs(void);
static esp_err_t prepare_keyboard_info_gui(struct menu_item *self);
static esp_err_t post_keyboard_info_gui(struct menu_item *self);

void keyboard_gui_init_keyboard_stats(void)
{
    // Initialize statistics
    memset(&s_keyboard_stats, 0, sizeof(keyboard_stats_t));
    s_keyboard_stats.session_start_time = esp_timer_get_time() / 1000; // Convert to ms

    // Load total count from NVS
    esp_err_t ret = load_total_count_from_nvs();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to load total count from NVS: %s, starting from 0", esp_err_to_name(ret));
        s_keyboard_stats.total_char_count = 0;
    } else {
        ESP_LOGI(TAG, "Loaded total count from NVS: %lu", s_keyboard_stats.total_char_count);
    }

    // Register event handler for NVS save operations
    ret = drv_loop_register_handler(KEYBOARD_STATS_EVENTS, SAVE_TOTAL_COUNT_EVENT, save_total_count_event_handler, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register save total count event handler: %s", esp_err_to_name(ret));
    }

    // Create and start the periodic save timer
    const esp_timer_create_args_t save_timer_args = {
        .callback = &save_total_count_timer_func,
        .name = "save_total_count_timer"
    };

    ret = esp_timer_create(&save_timer_args, &s_save_total_count_timer);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create save total count timer: %s", esp_err_to_name(ret));
    } else {
        ret = esp_timer_start_periodic(s_save_total_count_timer, SAVE_TOTAL_COUNT_PERIOD_MS * 1000);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to start save total count timer: %s", esp_err_to_name(ret));
        } else {
            ESP_LOGI(TAG, "Started periodic save timer (every %d seconds)", SAVE_TOTAL_COUNT_PERIOD_MS / 1000);
        }
    }

    // Create main screen
    lv_obj_set_style_bg_color(lv_screen_active(), lv_color_black(), 0);
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

static keyboard_info_gui_t* create_keyboard_info_gui(void)
{
    keyboard_info_gui_t *gui = malloc(sizeof(keyboard_info_gui_t));
    if (!gui) {
        ESP_LOGE(TAG, "Failed to allocate keyboard info GUI");
        return NULL;
    }

    // Create container for keyboard info
    gui->container = lv_obj_create(lv_screen_active());
    lv_obj_set_size(gui->container, LCD_WIDTH, LCD_HEIGHT);
    lv_obj_set_pos(gui->container, 0, 0);
    lv_obj_set_style_bg_color(gui->container, lv_color_black(), 0);
    lv_obj_set_style_border_width(gui->container, 0, 0);
    lv_obj_set_style_pad_all(gui->container, 20, 0);

    // Disable scrollbars for keyboard info container
    lv_obj_clear_flag(gui->container, LV_OBJ_FLAG_SCROLLABLE);

    // Hide container initially - will be shown when prepare_gui_func is called
    lv_obj_add_flag(gui->container, LV_OBJ_FLAG_HIDDEN);

    // Set horizontal flex layout for the container - all elements in a row
    lv_obj_set_flex_flow(gui->container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(gui->container, LV_FLEX_ALIGN_SPACE_AROUND, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // Total count icon - directly in main container
    gui->total_count_icon = lv_image_create(gui->container);
    lv_image_set_src(gui->total_count_icon, &keyboard_meter_A);
    lv_obj_set_style_margin_right(gui->total_count_icon, 8, 0);  // Small gap to count label

    // Total count label - directly in main container
    gui->total_count_label = lv_label_create(gui->container);
    lv_obj_set_style_text_color(gui->total_count_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(gui->total_count_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_align(gui->total_count_label, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_style_margin_right(gui->total_count_label, 30, 0);  // Gap between total and session
    lv_label_set_text(gui->total_count_label, "0");

    // Session count icon - directly in main container
    gui->session_count_icon = lv_image_create(gui->container);
    lv_image_set_src(gui->session_count_icon, &keyboard_meter_B);
    lv_obj_set_style_margin_right(gui->session_count_icon, 8, 0);  // Small gap to count label

    // Session count label - directly in main container
    gui->char_count_label = lv_label_create(gui->container);
    lv_obj_set_style_text_color(gui->char_count_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(gui->char_count_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_align(gui->char_count_label, LV_TEXT_ALIGN_LEFT, 0);
    lv_label_set_text(gui->char_count_label, "0");

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

static void update_keyboard_info_display(keyboard_info_gui_t *gui)
{
    if (!gui) {
        return;
    }

    char buffer[32];

    // Update total count - just show the number without "Total:" prefix
    snprintf(buffer, sizeof(buffer), "%lu", s_keyboard_stats.total_char_count);
    lv_label_set_text(gui->total_count_label, buffer);

    // Update session character count - just show the number without "Chars:" prefix
    snprintf(buffer, sizeof(buffer), "%lu", s_keyboard_stats.session_char_count);
    lv_label_set_text(gui->char_count_label, buffer);
}

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

// NVS save functions
static void save_total_count_timer_func(void *arg)
{
    esp_err_t ret = drv_loop_post_event(
        KEYBOARD_STATS_EVENTS,
        SAVE_TOTAL_COUNT_EVENT,
        NULL,
        0,
        0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to post save total count event: %s", esp_err_to_name(ret));
    }
}

static void save_total_count_event_handler(void *arg, esp_event_base_t base, int32_t id, void *event_data)
{
    if (base == KEYBOARD_STATS_EVENTS && id == SAVE_TOTAL_COUNT_EVENT) {
        esp_err_t ret = save_total_count_to_nvs();
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to save total count to NVS: %s", esp_err_to_name(ret));
        } else {
            ESP_LOGD(TAG, "Successfully saved total count %lu to NVS", s_keyboard_stats.total_char_count);
        }
    }
}

static esp_err_t load_total_count_from_nvs(void)
{
    size_t required_size = sizeof(uint32_t);
    esp_err_t ret = nvs_read_blob(NVS_NAMESPACE, NVS_TOTAL_COUNT_KEY, &s_keyboard_stats.total_char_count, &required_size);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read total count from NVS: %s", esp_err_to_name(ret));
        return ret;
    }

    return ESP_OK;
}

static esp_err_t save_total_count_to_nvs(void)
{
    static uint32_t total_count = 0;
    if (total_count != s_keyboard_stats.total_char_count) {
        total_count = s_keyboard_stats.total_char_count;
        esp_err_t ret = nvs_write_blob(NVS_NAMESPACE, NVS_TOTAL_COUNT_KEY, &total_count, sizeof(uint32_t));
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to write total count to NVS: %s", esp_err_to_name(ret));
            return ret;
        }
    }

    return ESP_OK;
}

// ============================================================================
// Reset Meter Implementation
// ============================================================================

esp_err_t keyboard_gui_prepare_reset_meter(struct menu_item *self)
{
    ESP_LOGI(TAG, "Preparing reset meter GUI");

    if (!self) {
        return ESP_ERR_INVALID_ARG;
    }

    // Create container for reset meter GUI if not already created
    if (!self->user_ctx) {
        lv_obj_t *container = lv_obj_create(lv_screen_active());
        lv_obj_set_size(container, LCD_WIDTH, LCD_HEIGHT);
        lv_obj_set_pos(container, 0, 0);
        lv_obj_set_style_bg_color(container, lv_color_black(), 0);
        lv_obj_set_style_border_width(container, 0, 0);
        lv_obj_set_style_pad_all(container, 0, 0);

        // Disable scrollbars for container
        lv_obj_clear_flag(container, LV_OBJ_FLAG_SCROLLABLE);

        // Create banner image
        lv_obj_t *banner_img = lv_image_create(container);
        lv_image_set_src(banner_img, &keyboard_reset_meter_banner);
        lv_obj_center(banner_img);

        self->user_ctx = container;
        ESP_LOGI(TAG, "Created new reset meter GUI with banner");
    }

    // Show the container
    lv_obj_remove_flag((lv_obj_t *)self->user_ctx, LV_OBJ_FLAG_HIDDEN);
    ESP_LOGI(TAG, "Showed reset meter container");

    // Perform the reset operation
    keyboard_gui_reset_session_stats();
    ESP_LOGI(TAG, "Reset session statistics");

    return ESP_OK;
}

esp_err_t keyboard_gui_reset_meter_action(void *user_ctx)
{
    ESP_LOGI(TAG, "Executing reset meter action");

    if (!user_ctx) {
        return ESP_ERR_INVALID_ARG;
    }

    // Perform the actual reset operation
    keyboard_gui_reset_session_stats();
    ESP_LOGI(TAG, "Session statistics reset via user action");

    return ESP_OK;
}

esp_err_t keyboard_gui_post_reset_meter(struct menu_item *self)
{
    ESP_LOGD(TAG, "Post reset meter GUI cleanup");

    if (!self || !self->user_ctx) {
        return ESP_OK;
    }

    // Hide the container
    lv_obj_add_flag((lv_obj_t *)self->user_ctx, LV_OBJ_FLAG_HIDDEN);
    ESP_LOGI(TAG, "Hidden reset meter container");

    return ESP_OK;
}

// ============================================================================
// Public API functions for menu integration
// ============================================================================

esp_err_t keyboard_gui_prepare_keyboard_info(struct menu_item *self)
{
    return prepare_keyboard_info_gui(self);
}

esp_err_t keyboard_gui_post_keyboard_info(struct menu_item *self)
{
    return post_keyboard_info_gui(self);
}
