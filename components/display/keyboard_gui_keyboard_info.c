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
#include "display_hardware_info.h"
#include "menu_state_machine.h"
#include "keyboard_gui_widgets.h"
#include "menu_icons.h"
#include "drv_loop.h"
#include "nvs_io.h"
#include "nvs.h"
#include "function_control.h"
#include "miscs.h"
#include "host.h"

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
    // Combined main section containing all elements
    lv_obj_t *main_container;
    lv_obj_t *counter_container;      // Container for the three counter labels (leftmost)
    lv_obj_t *session_counter_label;  // Session counter (top line)
    lv_obj_t *divider_label;          // "/" divider (middle line)
    lv_obj_t *total_counter_label;    // Total counter (bottom line)
    lv_obj_t *caps_lock_img;          // Caps lock status (second position)
    lv_obj_t *wifi_status_img;        // WiFi status (third position)
    lv_obj_t *ble_status_img;         // BLE status (fourth position)
    lv_obj_t *power_status_img;       // Power status (fifth position)
    lv_obj_t *battery_percent_label;  // Battery percentage (follows power status)

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

    // Create combined main container (full screen)
    gui->main_container = lv_obj_create(lv_screen_active());
    lv_obj_set_size(gui->main_container, LCD_WIDTH, LCD_HEIGHT);  // Use full screen
    lv_obj_set_pos(gui->main_container, 0, 0);  // Position at top
    lv_obj_set_style_bg_color(gui->main_container, lv_color_black(), 0);
    lv_obj_set_style_border_width(gui->main_container, 0, 0);
    lv_obj_set_style_pad_all(gui->main_container, 5, 0);  // Small padding for spacing

    // Disable scrollbars completely
    lv_obj_clear_flag(gui->main_container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(gui->main_container, LV_DIR_NONE);

    // Hide container initially
    lv_obj_add_flag(gui->main_container, LV_OBJ_FLAG_HIDDEN);

    // Set up the main container as a flex container for the content
    lv_obj_set_flex_flow(gui->main_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(gui->main_container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(gui->main_container, 0, 0);  // Gap between items

    // Counter container and labels (session/total) - leftmost item
    gui->counter_container = lv_obj_create(gui->main_container);
    lv_obj_set_size(gui->counter_container, 60, 60);  // Fixed size container
    lv_obj_set_style_bg_opa(gui->counter_container, LV_OPA_TRANSP, 0);  // Transparent background
    lv_obj_set_style_border_width(gui->counter_container, 0, 0);  // No border
    lv_obj_set_style_pad_all(gui->counter_container, 0, 0);  // No padding
    lv_obj_clear_flag(gui->counter_container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(gui->counter_container, LV_FLEX_FLOW_COLUMN);  // Changed to vertical layout
    lv_obj_set_flex_align(gui->counter_container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(gui->counter_container, 1, 0);  // 1 pixel gap between items vertically

    // Session counter label (top)
    gui->session_counter_label = lv_label_create(gui->counter_container);
    lv_obj_set_style_text_color(gui->session_counter_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(gui->session_counter_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_align(gui->session_counter_label, LV_TEXT_ALIGN_CENTER, 0);  // Center align for vertical layout
    lv_obj_set_width(gui->session_counter_label, 80);  // Full width of container
    lv_label_set_text(gui->session_counter_label, "0");

    // Total counter label (bottom)
    gui->total_counter_label = lv_label_create(gui->counter_container);
    lv_obj_set_style_text_color(gui->total_counter_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(gui->total_counter_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_align(gui->total_counter_label, LV_TEXT_ALIGN_CENTER, 0);  // Center align for vertical layout
    lv_obj_set_width(gui->total_counter_label, 80);  // Full width of container
    lv_label_set_text(gui->total_counter_label, "0");

    // Caps lock status icon - second item
    gui->caps_lock_img = lv_image_create(gui->main_container);
    lv_image_set_src(gui->caps_lock_img, &lower_case_letter);

    // WiFi status icon - third item
    gui->wifi_status_img = lv_image_create(gui->main_container);
    lv_image_set_src(gui->wifi_status_img, &wifi_off);

    // BLE status icon - fourth item
    gui->ble_status_img = lv_image_create(gui->main_container);
    lv_image_set_src(gui->ble_status_img, &bluetooth_off);

    // Power status icon - fifth item
    gui->power_status_img = lv_image_create(gui->main_container);
    lv_image_set_src(gui->power_status_img, &battery_normal);

    // Battery percentage label - follows power status
    gui->battery_percent_label = lv_label_create(gui->main_container);
    lv_obj_set_style_text_color(gui->battery_percent_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(gui->battery_percent_label, &lv_font_montserrat_10, 0);
    lv_obj_set_width(gui->battery_percent_label, 28);  // Width for percentage text
    lv_obj_set_style_text_align(gui->battery_percent_label, LV_TEXT_ALIGN_CENTER, 0);  // Center align text
    lv_label_set_text(gui->battery_percent_label, "100%");

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

    // Update WiFi status
    if (is_wifi_enabled()) {
        lv_image_set_src(gui->wifi_status_img, &wifi_on);
    } else {
        lv_image_set_src(gui->wifi_status_img, &wifi_off);
    }

    // Update BLE status
    if (is_ble_enabled()) {
        lv_image_set_src(gui->ble_status_img, &bluetooth_on);
    } else {
        lv_image_set_src(gui->ble_status_img, &bluetooth_off);
    }

    // Update power status and battery percentage
    bool is_usb_powered = miscs_is_usb_powered();
    bool is_charging = miscs_is_battery_charging();
    uint8_t battery_percent = 0;

    // Get battery percentage
    miscs_get_battery_percentage(&battery_percent, false);

    if (is_usb_powered && !is_charging) {
        // USB powered, not charging
        lv_image_set_src(gui->power_status_img, &plug_on);
        // Show USB indicator instead of hiding battery percentage
        lv_obj_remove_flag(gui->battery_percent_label, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(gui->battery_percent_label, "");
    } else if (is_charging) {
        // Charging
        lv_image_set_src(gui->power_status_img, &battery_charge);
        lv_obj_remove_flag(gui->battery_percent_label, LV_OBJ_FLAG_HIDDEN);
        snprintf(buffer, sizeof(buffer), "%u%%", battery_percent);
        lv_label_set_text(gui->battery_percent_label, buffer);
    } else {
        // Battery powered
        lv_image_set_src(gui->power_status_img, &battery_normal);
        lv_obj_remove_flag(gui->battery_percent_label, LV_OBJ_FLAG_HIDDEN);
        snprintf(buffer, sizeof(buffer), "%u%%", battery_percent);
        lv_label_set_text(gui->battery_percent_label, buffer);
    }

    // Update caps lock status
    led_t host_leds = host_keyboard_led_state();
    if (host_leds.caps_lock) { // USB_LED_CAPS_LOCK is bit 1
        lv_image_set_src(gui->caps_lock_img, &upper_case_letter);
    } else {
        lv_image_set_src(gui->caps_lock_img, &lower_case_letter);
    }

    // Update counter display using three separate labels
    snprintf(buffer, sizeof(buffer), "S: %lu", s_keyboard_stats.session_char_count);
    lv_label_set_text(gui->session_counter_label, buffer);

    snprintf(buffer, sizeof(buffer), "T: %lu", s_keyboard_stats.total_char_count);
    lv_label_set_text(gui->total_counter_label, buffer);
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

    // Show the combined main container
    lv_obj_remove_flag(gui->main_container, LV_OBJ_FLAG_HIDDEN);
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

    // Hide the combined main container
    lv_obj_add_flag(gui->main_container, LV_OBJ_FLAG_HIDDEN);
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

        // Disable scrollbars completely for container
        lv_obj_clear_flag(container, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_scroll_dir(container, LV_DIR_NONE);

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
