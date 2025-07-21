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
#include "esp_err.h"
#include "esp_log.h"
#include "lvgl.h"
#include "lcd_hardware.h"
#include "menu_state_machine.h"
#include "keyboard_gui_widgets.h"
#include "menu_icons.h"

#define TAG "gui_about"

// GUI context structure for about screen
typedef struct {
    lv_obj_t *container;
    lv_obj_t *title_label;
    lv_obj_t *info_label;
    lv_obj_t *version_label;
    lv_obj_t *author_label;
} about_gui_t;

// Static function declarations
static about_gui_t* create_about_gui(void);
static esp_err_t prepare_about_gui(struct menu_item *self);
static esp_err_t post_about_gui(struct menu_item *self);

static about_gui_t* create_about_gui(void)
{
    ESP_LOGI(TAG, "Creating about GUI");

    about_gui_t *gui = malloc(sizeof(about_gui_t));
    if (!gui) {
        ESP_LOGE(TAG, "Failed to allocate memory for about GUI");
        return NULL;
    }

    // Create main container
    gui->container = lv_obj_create(lv_screen_active());
    lv_obj_set_size(gui->container, LCD_WIDTH, LCD_HEIGHT);
    lv_obj_set_pos(gui->container, 0, 0);
    lv_obj_set_style_bg_color(gui->container, lv_color_black(), 0);
    lv_obj_set_style_border_width(gui->container, 0, 0);
    lv_obj_set_style_pad_all(gui->container, 20, 0);

    // Disable scrollbars for about container
    lv_obj_clear_flag(gui->container, LV_OBJ_FLAG_SCROLLABLE);

    // Hide container initially - will be shown when prepare_gui_func is called
    lv_obj_add_flag(gui->container, LV_OBJ_FLAG_HIDDEN);

    // Set vertical flex layout for the container - all elements in a column
    lv_obj_set_flex_flow(gui->container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(gui->container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // Title label
    gui->title_label = lv_label_create(gui->container);
    lv_obj_set_style_text_color(gui->title_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(gui->title_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_align(gui->title_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_margin_bottom(gui->title_label, 20, 0);
    lv_label_set_text(gui->title_label, "MK32 Keyboard");

    // Info description
    gui->info_label = lv_label_create(gui->container);
    lv_obj_set_style_text_color(gui->info_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(gui->info_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_align(gui->info_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_margin_bottom(gui->info_label, 15, 0);
    lv_obj_set_width(gui->info_label, LCD_WIDTH - 40); // Leave some padding
    lv_label_set_long_mode(gui->info_label, LV_LABEL_LONG_WRAP);
    lv_label_set_text(gui->info_label,
        "A custom keyboard firmware for ESP32\n"
        "inspired by MK32 and QMK firmware.\n\n"
        "Features wireless connectivity,\n"
        "custom key mappings, and\n"
        "real-time statistics.");

    // Version info
    gui->version_label = lv_label_create(gui->container);
    lv_obj_set_style_text_color(gui->version_label, lv_color_hex(0x808080), 0); // Gray color
    lv_obj_set_style_text_font(gui->version_label, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_align(gui->version_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_margin_bottom(gui->version_label, 10, 0);
    lv_label_set_text(gui->version_label, "Version 1.0.0");

    // Author info
    gui->author_label = lv_label_create(gui->container);
    lv_obj_set_style_text_color(gui->author_label, lv_color_hex(0x808080), 0); // Gray color
    lv_obj_set_style_text_font(gui->author_label, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_align(gui->author_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(gui->author_label, "Copyright (C) 2024 github.com/paul356");

    ESP_LOGI(TAG, "About GUI created successfully");
    return gui;
}

static esp_err_t prepare_about_gui(struct menu_item *self)
{
    ESP_LOGI(TAG, "Preparing about GUI");

    if (!self) {
        return ESP_ERR_INVALID_ARG;
    }

    // Create about GUI if not already created
    if (!self->user_ctx) {
        self->user_ctx = create_about_gui();
        if (!self->user_ctx) {
            ESP_LOGE(TAG, "Failed to create about GUI");
            return ESP_ERR_NO_MEM;
        }
        ESP_LOGI(TAG, "Created new about GUI");
    }

    about_gui_t *gui = (about_gui_t *)self->user_ctx;

    // Show the about container
    lv_obj_remove_flag(gui->container, LV_OBJ_FLAG_HIDDEN);
    ESP_LOGI(TAG, "Showed about container");

    return ESP_OK;
}

static esp_err_t post_about_gui(struct menu_item *self)
{
    ESP_LOGD(TAG, "Post about GUI cleanup");

    if (!self || !self->user_ctx) {
        return ESP_OK;
    }

    about_gui_t *gui = (about_gui_t *)self->user_ctx;

    // Hide the about container
    lv_obj_add_flag(gui->container, LV_OBJ_FLAG_HIDDEN);
    ESP_LOGI(TAG, "Hidden about container");

    return ESP_OK;
}

// ============================================================================
// Public API functions for menu integration
// ============================================================================

esp_err_t keyboard_gui_prepare_about(struct menu_item *self)
{
    return prepare_about_gui(self);
}

esp_err_t keyboard_gui_post_about(struct menu_item *self)
{
    return post_about_gui(self);
}
