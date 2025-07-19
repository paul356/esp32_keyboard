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
#include "lvgl.h"
#include "lcd_hardware.h"
#include "menu_state_machine.h"
#include "keyboard_gui_widgets.h"
#include "menu_icons.h"
#include "function_control.h"

#define TAG "gui_led"

// GUI context structure for LED functionality
typedef struct {
    lv_obj_t *container;
    lv_obj_t *led_icon;
    lv_obj_t *status_label;
} led_toggle_gui_t;

// Static function declarations
static led_toggle_gui_t* create_led_toggle_gui(void);
static esp_err_t prepare_led_toggle_gui(struct menu_item *self);
static esp_err_t post_led_toggle_gui(struct menu_item *self);

static led_toggle_gui_t* create_led_toggle_gui(void)
{
    led_toggle_gui_t *gui = malloc(sizeof(led_toggle_gui_t));
    if (!gui) {
        ESP_LOGE(TAG, "Failed to allocate memory for LED toggle GUI");
        return NULL;
    }

    // Create container for led_toggle interface
    gui->container = lv_obj_create(lv_screen_active());
    lv_obj_set_size(gui->container, LCD_WIDTH, LCD_HEIGHT);
    lv_obj_set_pos(gui->container, 0, 0);
    lv_obj_set_style_bg_color(gui->container, lv_color_black(), 0);
    lv_obj_set_style_border_width(gui->container, 0, 0);
    lv_obj_set_style_pad_all(gui->container, 20, 0);

    // Disable scrollbars for led_toggle container
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

    // LED icon - using the switch icon
    gui->led_icon = lv_image_create(gui->container);
    lv_image_set_src(gui->led_icon, &switch_icon);

    // Set initial label text and icon appearance based on LED state (assuming ON for now)
    lv_label_set_text(gui->status_label, "Close");
    // LED is ON, so show normal icon color
    lv_obj_set_style_image_recolor_opa(gui->led_icon, LV_OPA_TRANSP, 0);

    return gui;
}

static esp_err_t prepare_led_toggle_gui(struct menu_item *self)
{
    if (!self) {
        ESP_LOGE(TAG, "Cannot prepare LED toggle GUI: menu item is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Preparing LED toggle GUI");

    // Create GUI if it doesn't exist
    if (!self->user_ctx) {
        self->user_ctx = create_led_toggle_gui();
        if (!self->user_ctx) {
            ESP_LOGE(TAG, "Failed to create LED toggle GUI");
            return ESP_ERR_NO_MEM;
        }
    }

    led_toggle_gui_t *gui = (led_toggle_gui_t *)self->user_ctx;

    // Update LED status display (assuming LED is ON for now)
    lv_label_set_text(gui->status_label, "Close");
    lv_obj_set_style_image_recolor_opa(gui->led_icon, LV_OPA_TRANSP, 0);

    // Show the GUI
    lv_obj_clear_flag(gui->container, LV_OBJ_FLAG_HIDDEN);

    ESP_LOGI(TAG, "LED toggle GUI prepared and displayed");
    return ESP_OK;
}

static esp_err_t post_led_toggle_gui(struct menu_item *self)
{
    if (!self || !self->user_ctx) {
        ESP_LOGE(TAG, "Cannot cleanup LED toggle GUI: invalid context");
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Cleaning up LED toggle GUI");

    led_toggle_gui_t *gui = (led_toggle_gui_t *)self->user_ctx;

    // Hide the GUI
    lv_obj_add_flag(gui->container, LV_OBJ_FLAG_HIDDEN);

    return ESP_OK;
}

// ============================================================================
// Public API functions for menu integration
// ============================================================================

esp_err_t keyboard_gui_prepare_led_toggle(struct menu_item *self)
{
    return prepare_led_toggle_gui(self);
}

esp_err_t keyboard_gui_post_led_toggle(struct menu_item *self)
{
    return post_led_toggle_gui(self);
}

esp_err_t keyboard_gui_led_toggle_action(void *user_ctx)
{
    ESP_LOGI(TAG, "LED toggle action triggered");

    if (!user_ctx) {
        ESP_LOGE(TAG, "Invalid LED toggle user context");
        return ESP_ERR_INVALID_ARG;
    }

    led_toggle_gui_t *gui = (led_toggle_gui_t *)user_ctx;

    // For now, we'll just toggle the display between "Close" and "Open"
    // In a real implementation, you would check actual LED hardware status
    const char *current_text = lv_label_get_text(gui->status_label);

    if (strcmp(current_text, "Close") == 0) {
        // Currently ON, turn OFF
        ESP_LOGI(TAG, "Turning LED OFF");
        lv_label_set_text(gui->status_label, "Open");
        // Gray out the icon to indicate OFF state
        lv_obj_set_style_image_recolor_opa(gui->led_icon, LV_OPA_50, 0);
        lv_obj_set_style_image_recolor(gui->led_icon, lv_color_hex(0x808080), 0);

        update_led_switch(false);
    } else {
        // Currently OFF, turn ON
        ESP_LOGI(TAG, "Turning LED ON");
        lv_label_set_text(gui->status_label, "Close");
        // Restore normal icon color
        lv_obj_set_style_image_recolor_opa(gui->led_icon, LV_OPA_TRANSP, 0);

        update_led_switch(true);
    }

    ESP_LOGI(TAG, "LED toggle action completed successfully");
    return ESP_OK;
}
