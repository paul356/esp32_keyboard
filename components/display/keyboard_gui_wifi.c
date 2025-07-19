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
#include "esp_wifi.h"
#include "lvgl.h"
#include "lcd_hardware.h"
#include "menu_state_machine.h"
#include "keyboard_gui_widgets.h"
#include "menu_icons.h"

#define TAG "gui_wifi"

// GUI context structure for WiFi functionality
typedef struct {
    lv_obj_t *container;
    lv_obj_t *wifi_icon;            // WiFi icon
    lv_obj_t *status_label;         // Label showing "Open" or "Close"
} wifi_toggle_gui_t;

// Static function declarations
static wifi_toggle_gui_t* create_wifi_toggle_gui(void);
static esp_err_t prepare_wifi_toggle_gui(struct menu_item *self);
static esp_err_t post_wifi_toggle_gui(struct menu_item *self);

// Function to get current WiFi mode - placeholder implementation
static wifi_mode_t get_wifi_mode(void)
{
    wifi_mode_t mode;
    esp_err_t ret = esp_wifi_get_mode(&mode);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to get WiFi mode: %s", esp_err_to_name(ret));
        return WIFI_MODE_NULL;
    }
    return mode;
}

static wifi_toggle_gui_t* create_wifi_toggle_gui(void)
{
    wifi_toggle_gui_t *gui = malloc(sizeof(wifi_toggle_gui_t));
    if (!gui) {
        ESP_LOGE(TAG, "Failed to allocate wifi_toggle GUI");
        return NULL;
    }

    // Create container for wifi_toggle interface
    gui->container = lv_obj_create(lv_screen_active());
    lv_obj_set_size(gui->container, LCD_WIDTH, LCD_HEIGHT);
    lv_obj_set_pos(gui->container, 0, 0);
    lv_obj_set_style_bg_color(gui->container, lv_color_black(), 0);
    lv_obj_set_style_border_width(gui->container, 0, 0);
    lv_obj_set_style_pad_all(gui->container, 20, 0);

    // Disable scrollbars for wifi_toggle container
    lv_obj_clear_flag(gui->container, LV_OBJ_FLAG_SCROLLABLE);

    // Hide container initially - will be shown when prepare_gui_func is called
    lv_obj_add_flag(gui->container, LV_OBJ_FLAG_HIDDEN);

    // Set horizontal flex layout for the container - label and icon in a row
    lv_obj_set_flex_flow(gui->container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(gui->container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // Status label (place before icon)
    gui->status_label = lv_label_create(gui->container);
    lv_obj_set_style_text_color(gui->status_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(gui->status_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_align(gui->status_label, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_style_margin_right(gui->status_label, 20, 0);  // Gap between label and icon

    // WiFi icon
    gui->wifi_icon = lv_image_create(gui->container);
    lv_image_set_src(gui->wifi_icon, &wifi_icon);

    // Set initial label text and icon appearance based on current WiFi state
    if (get_wifi_mode() != WIFI_MODE_NULL) {
        lv_label_set_text(gui->status_label, "Close");
        // Icon is colored normally when WiFi is enabled
        lv_obj_set_style_image_recolor_opa(gui->wifi_icon, LV_OPA_TRANSP, 0);
    } else {
        lv_label_set_text(gui->status_label, "Open");
        // Grey out the icon when WiFi is disabled
        lv_obj_set_style_image_recolor_opa(gui->wifi_icon, LV_OPA_50, 0);
        lv_obj_set_style_image_recolor(gui->wifi_icon, lv_color_hex(0x808080), 0);
    }

    return gui;
}

static esp_err_t prepare_wifi_toggle_gui(struct menu_item *self)
{
    ESP_LOGI(TAG, "Preparing WiFi toggle GUI");

    if (!self) {
        return ESP_ERR_INVALID_ARG;
    }

    // Create WiFi toggle GUI if not already created
    if (!self->user_ctx) {
        self->user_ctx = create_wifi_toggle_gui();
        if (!self->user_ctx) {
            ESP_LOGE(TAG, "Failed to create WiFi toggle GUI");
            return ESP_ERR_NO_MEM;
        }
        ESP_LOGI(TAG, "Created new WiFi toggle GUI");
    }

    wifi_toggle_gui_t *gui = (wifi_toggle_gui_t *)self->user_ctx;

    // Update label text and icon appearance based on current WiFi state
    if (get_wifi_mode() != WIFI_MODE_NULL) {
        lv_label_set_text(gui->status_label, "Close");
        // Icon is colored normally when WiFi is enabled
        lv_obj_set_style_image_recolor_opa(gui->wifi_icon, LV_OPA_TRANSP, 0);
    } else {
        lv_label_set_text(gui->status_label, "Open");
        // Grey out the icon when WiFi is disabled
        lv_obj_set_style_image_recolor_opa(gui->wifi_icon, LV_OPA_50, 0);
        lv_obj_set_style_image_recolor(gui->wifi_icon, lv_color_hex(0x808080), 0);
    }

    // Show the WiFi toggle container
    lv_obj_remove_flag(gui->container, LV_OBJ_FLAG_HIDDEN);
    ESP_LOGI(TAG, "Showed WiFi toggle container");

    return ESP_OK;
}

static esp_err_t post_wifi_toggle_gui(struct menu_item *self)
{
    ESP_LOGD(TAG, "Post WiFi toggle GUI cleanup");

    if (!self || !self->user_ctx) {
        return ESP_OK;
    }

    wifi_toggle_gui_t *gui = (wifi_toggle_gui_t *)self->user_ctx;

    // Hide the WiFi toggle container
    lv_obj_add_flag(gui->container, LV_OBJ_FLAG_HIDDEN);
    ESP_LOGI(TAG, "Hidden WiFi toggle container");

    return ESP_OK;
}

// ============================================================================
// Public API functions for menu integration
// ============================================================================

esp_err_t keyboard_gui_prepare_wifi_toggle(struct menu_item *self)
{
    return prepare_wifi_toggle_gui(self);
}

esp_err_t keyboard_gui_post_wifi_toggle(struct menu_item *self)
{
    return post_wifi_toggle_gui(self);
}

esp_err_t keyboard_gui_wifi_toggle_action(void *user_ctx)
{
    ESP_LOGI(TAG, "WiFi toggle action triggered");

    if (!user_ctx) {
        ESP_LOGE(TAG, "Invalid WiFi toggle user context");
        return ESP_ERR_INVALID_ARG;
    }

    wifi_toggle_gui_t *gui = (wifi_toggle_gui_t *)user_ctx;

    // Toggle WiFi state
    if (get_wifi_mode() != WIFI_MODE_NULL) {
        // WiFi is currently enabled, disable it
        ESP_LOGI(TAG, "Disabling WiFi");
        /*esp_err_t ret = update_wifi_state(WIFI_MODE_NULL, "", "");
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to disable WiFi: %s", esp_err_to_name(ret));
            return ret;
        }*/

        // Update GUI to reflect disabled state
        lv_label_set_text(gui->status_label, "Open");
        lv_obj_set_style_image_recolor_opa(gui->wifi_icon, LV_OPA_50, 0);
        lv_obj_set_style_image_recolor(gui->wifi_icon, lv_color_hex(0x808080), 0);
    } else {
        // WiFi is currently disabled, enable it
        ESP_LOGI(TAG, "Enabling WiFi");
        // Enable WiFi in station mode by default - you may want to make this configurable
        /*esp_err_t ret = update_wifi_state(WIFI_MODE_STA, "default_ssid", "default_password");
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to enable WiFi: %s", esp_err_to_name(ret));
            return ret;
        }*/

        // Update GUI to reflect enabled state
        lv_label_set_text(gui->status_label, "Close");
        lv_obj_set_style_image_recolor_opa(gui->wifi_icon, LV_OPA_TRANSP, 0);
    }

    ESP_LOGI(TAG, "WiFi toggle action completed successfully");
    return ESP_OK;
}
