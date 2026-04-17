/*
 *  This program is a keyboard firmware for ESP family boards inspired by
 *  MK32 and qmk_firmware.
 *
 *  Copyright (C) 2025 panhao356@gmail.com
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
#include "display_hardware_info.h"
#include "menu_state_machine.h"
#include "keyboard_gui_widgets.h"
#include "menu_icons.h"
#include "function_control.h"
#include "led_ctrl.h"

#define TAG "gui_led"

// GUI context structure for LED functionality
typedef struct {
    lv_obj_t *container;
    lv_obj_t *led_icon;
    lv_obj_t *status_label;
} led_toggle_gui_t;

// LED pattern settings GUI focus states
typedef enum {
    WIDGET_FOCUS_LED_PATTERN = 0,  // Pattern selector focus
    WIDGET_FOCUS_LED_PARAM1 = 1,   // First parameter text field focus
    WIDGET_FOCUS_LED_PARAM2 = 2,   // Second parameter text field focus
    WIDGET_FOCUS_COUNT_PATTERN = 3 // Total number of focusable elements
} led_pattern_widget_focus_e;

// LED pattern definitions with human-readable names - mapped to led_pattern_type_e
static const char* led_pattern_names[] = {
    "Off",           // LED_PATTERN_OFF
    "Hit Key"        // LED_PATTERN_HIT_KEY
};

#define LED_PATTERN_COUNT (LED_PATTERN_MAX)

// LED pattern settings GUI context structure
typedef struct {
    lv_obj_t *container;
    lv_obj_t *pattern_section;      // Container for pattern roller and label
    lv_obj_t *pattern_roller;       // Roller for pattern selection
    lv_obj_t *param1_textfield;     // Text field for first parameter (speed/hue)
    lv_obj_t *param2_textfield;     // Text field for second parameter (saturation/brightness)
    lv_obj_t *pattern_label;        // Label for pattern selector
    lv_obj_t *param1_label;         // Label for first parameter field
    lv_obj_t *param2_label;         // Label for second parameter field
    led_pattern_widget_focus_e current_focus; // Current focus state
    int cursor_pos_param1;          // Cursor position in param1 field
    int cursor_pos_param2;          // Cursor position in param2 field
    char param1_buffer[8];          // First parameter input buffer
    char param2_buffer[8];          // Second parameter input buffer
    int selected_pattern;           // Selected pattern index
} led_pattern_settings_gui_t;

// Static function declarations
static led_toggle_gui_t* create_led_toggle_gui(void);
static esp_err_t prepare_led_toggle_gui(struct menu_item *self);
static esp_err_t post_led_toggle_gui(struct menu_item *self);
static led_pattern_settings_gui_t* create_led_pattern_settings_gui(void);
static esp_err_t prepare_led_pattern_settings_gui(struct menu_item *self);
static esp_err_t post_led_pattern_settings_gui(struct menu_item *self);
static bool led_pattern_settings_handle_input_key(void *user_ctx, input_event_e input_event, char key_code);
static void update_led_pattern_focus_style(led_pattern_settings_gui_t *gui);
static void update_led_pattern_cursor_display(led_pattern_settings_gui_t *gui);
static void initialize_led_pattern_focus_styling(led_pattern_settings_gui_t *gui);

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

    // Set initial label text and icon appearance based on actual LED state
    bool led_enabled = is_led_enabled();
    if (led_enabled) {
        lv_label_set_text(gui->status_label, "Close");
        // LED is ON, so show normal icon color
        lv_obj_set_style_image_recolor_opa(gui->led_icon, LV_OPA_TRANSP, 0);
    } else {
        lv_label_set_text(gui->status_label, "Open");
        // LED is OFF, so gray out the icon
        lv_obj_set_style_image_recolor_opa(gui->led_icon, LV_OPA_50, 0);
        lv_obj_set_style_image_recolor(gui->led_icon, lv_color_hex(0x808080), 0);
    }

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

    // Update LED status display based on actual hardware status
    bool led_enabled = is_led_enabled();
    if (led_enabled) {
        ESP_LOGI(TAG, "LED is currently ON");
        lv_label_set_text(gui->status_label, "Close");
        // LED is ON, so show normal icon color
        lv_obj_set_style_image_recolor_opa(gui->led_icon, LV_OPA_TRANSP, 0);
    } else {
        ESP_LOGI(TAG, "LED is currently OFF");
        lv_label_set_text(gui->status_label, "Open");
        // LED is OFF, so gray out the icon
        lv_obj_set_style_image_recolor_opa(gui->led_icon, LV_OPA_50, 0);
        lv_obj_set_style_image_recolor(gui->led_icon, lv_color_hex(0x808080), 0);
    }

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

    // Get current LED hardware status
    bool current_led_state = is_led_enabled();

    if (current_led_state) {
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

static led_pattern_settings_gui_t* create_led_pattern_settings_gui(void)
{
    led_pattern_settings_gui_t *gui = malloc(sizeof(led_pattern_settings_gui_t));
    if (!gui) {
        ESP_LOGE(TAG, "Failed to allocate led_pattern_settings GUI");
        return NULL;
    }

    // Initialize GUI state
    gui->current_focus = WIDGET_FOCUS_LED_PATTERN;
    gui->cursor_pos_param1 = 0;
    gui->cursor_pos_param2 = 0;
    gui->selected_pattern = LED_PATTERN_OFF; // Off by default
    strcpy(gui->param1_buffer, "100");  // Default speed/hue value
    strcpy(gui->param2_buffer, "255");  // Default saturation/brightness value

    // Create main container
    gui->container = lv_obj_create(lv_screen_active());
    if (!gui->container) {
        ESP_LOGE(TAG, "Failed to create main container");
        free(gui);
        return NULL;
    }
    lv_obj_set_size(gui->container, LCD_WIDTH, LCD_HEIGHT);
    lv_obj_set_pos(gui->container, 0, 0);
    lv_obj_set_style_bg_color(gui->container, lv_color_black(), 0);
    lv_obj_set_style_border_width(gui->container, 0, 0);
    lv_obj_set_style_pad_all(gui->container, 5, 0);
    lv_obj_clear_flag(gui->container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(gui->container, LV_OBJ_FLAG_HIDDEN);

    // Set horizontal layout to fit in 76px height
    lv_obj_set_flex_flow(gui->container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(gui->container, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // Pattern section - vertical label + roller for compact horizontal layout
    gui->pattern_section = lv_obj_create(gui->container);
    lv_obj_set_size(gui->pattern_section, 120, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(gui->pattern_section, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(gui->pattern_section, 0, 0);
    lv_obj_set_style_pad_all(gui->pattern_section, 2, 0);
    lv_obj_set_flex_flow(gui->pattern_section, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(gui->pattern_section, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    gui->pattern_label = lv_label_create(gui->pattern_section);
    lv_label_set_text(gui->pattern_label, "Pattern:");
    lv_obj_set_style_text_color(gui->pattern_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(gui->pattern_label, &lv_font_montserrat_12, 0);

    gui->pattern_roller = lv_roller_create(gui->pattern_section);
    if (!gui->pattern_roller) {
        ESP_LOGE(TAG, "Failed to create pattern roller");
        lv_obj_del(gui->container);
        free(gui);
        return NULL;
    }
    // Create options string from pattern names
    char *options = malloc(1024);
    if (!options) {
        ESP_LOGE(TAG, "Failed to allocate memory for pattern options");
        lv_obj_del(gui->container);
        free(gui);
        return NULL;
    }
    strcpy(options, led_pattern_names[0]);
    for (int i = 1; i < LED_PATTERN_COUNT; i++) {
        strcat(options, "\n");
        strcat(options, led_pattern_names[i]);
    }
    lv_roller_set_options(gui->pattern_roller, options, LV_ROLLER_MODE_NORMAL);
    free(options);
    lv_roller_set_visible_row_count(gui->pattern_roller, 1);
    lv_obj_set_size(gui->pattern_roller, 100, 25); // Set explicit height to make it compact
    lv_obj_set_style_bg_color(gui->pattern_roller, lv_color_hex(0x333333), 0);
    lv_obj_set_style_text_color(gui->pattern_roller, lv_color_white(), 0);
    lv_obj_set_style_pad_all(gui->pattern_roller, 2, 0); // Reduce internal padding
    lv_obj_set_style_text_font(gui->pattern_roller, &lv_font_montserrat_12, 0); // Use smaller font

    // Remove the blue background and white border from selected item
    lv_obj_set_style_bg_opa(gui->pattern_roller, LV_OPA_TRANSP, LV_PART_SELECTED);
    lv_obj_set_style_border_width(gui->pattern_roller, 0, LV_PART_SELECTED);
    lv_obj_set_style_outline_width(gui->pattern_roller, 0, LV_PART_SELECTED);

    // Create container for parameters - stacked vertically
    lv_obj_t *params_container = lv_obj_create(gui->container);
    lv_obj_set_size(params_container, 160, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(params_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(params_container, 0, 0);
    lv_obj_set_style_pad_all(params_container, 2, 0);
    lv_obj_set_flex_flow(params_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(params_container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // Parameter 1 section - horizontal label + field within the vertical stack
    lv_obj_t *param1_section = lv_obj_create(params_container);
    lv_obj_set_size(param1_section, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(param1_section, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(param1_section, 0, 0);
    lv_obj_set_style_pad_all(param1_section, 1, 0);
    lv_obj_set_flex_flow(param1_section, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(param1_section, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    gui->param1_label = lv_label_create(param1_section);
    lv_label_set_text(gui->param1_label, "Speed:");
    lv_obj_set_style_text_color(gui->param1_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(gui->param1_label, &lv_font_montserrat_12, 0);

    gui->param1_textfield = lv_textarea_create(param1_section);
    if (!gui->param1_textfield) {
        ESP_LOGE(TAG, "Failed to create param1 textfield");
        lv_obj_del(gui->container);
        free(gui);
        return NULL;
    }
    lv_textarea_set_one_line(gui->param1_textfield, true);
    lv_textarea_set_placeholder_text(gui->param1_textfield, "0-255");
    lv_obj_set_size(gui->param1_textfield, 80, 20); // Set explicit height for compactness
    lv_obj_set_style_text_color(gui->param1_textfield, lv_color_white(), 0);
    lv_obj_set_style_bg_color(gui->param1_textfield, lv_color_hex(0x333333), 0);
    lv_obj_set_style_pad_all(gui->param1_textfield, 2, 0); // Reduce padding
    lv_obj_set_style_text_font(gui->param1_textfield, &lv_font_montserrat_12, 0); // Use smaller font

    // Parameter 2 section - horizontal label + field within the vertical stack
    lv_obj_t *param2_section = lv_obj_create(params_container);
    lv_obj_set_size(param2_section, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(param2_section, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(param2_section, 0, 0);
    lv_obj_set_style_pad_all(param2_section, 1, 0);
    lv_obj_set_flex_flow(param2_section, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(param2_section, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    gui->param2_label = lv_label_create(param2_section);
    lv_label_set_text(gui->param2_label, "Brightness:");
    lv_obj_set_style_text_color(gui->param2_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(gui->param2_label, &lv_font_montserrat_12, 0);

    gui->param2_textfield = lv_textarea_create(param2_section);
    if (!gui->param2_textfield) {
        ESP_LOGE(TAG, "Failed to create param2 textfield");
        lv_obj_del(gui->container);
        free(gui);
        return NULL;
    }
    lv_textarea_set_one_line(gui->param2_textfield, true);
    lv_textarea_set_placeholder_text(gui->param2_textfield, "0-255");
    lv_obj_set_size(gui->param2_textfield, 80, 20); // Set explicit height for compactness
    lv_obj_set_style_text_color(gui->param2_textfield, lv_color_white(), 0);
    lv_obj_set_style_bg_color(gui->param2_textfield, lv_color_hex(0x333333), 0);
    lv_obj_set_style_pad_all(gui->param2_textfield, 2, 0); // Reduce padding
    lv_obj_set_style_text_font(gui->param2_textfield, &lv_font_montserrat_12, 0); // Use smaller font

    // Set initial values
    lv_textarea_set_text(gui->param1_textfield, gui->param1_buffer);
    lv_textarea_set_text(gui->param2_textfield, gui->param2_buffer);

    return gui;
}

static esp_err_t prepare_led_pattern_settings_gui(struct menu_item *self)
{
    ESP_LOGI(TAG, "Preparing LED pattern settings GUI");

    if (!self) {
        return ESP_ERR_INVALID_ARG;
    }

    // Create LED pattern settings GUI if not already created
    if (!self->user_ctx) {
        self->user_ctx = create_led_pattern_settings_gui();
        if (!self->user_ctx) {
            ESP_LOGE(TAG, "Failed to create LED pattern settings GUI");
            return ESP_ERR_NO_MEM;
        }
        ESP_LOGI(TAG, "Created new LED pattern settings GUI");
    }

    led_pattern_settings_gui_t *gui = (led_pattern_settings_gui_t *)self->user_ctx;

    // Initialize focus styling now that all functions are defined
    initialize_led_pattern_focus_styling(gui);

    // Update GUI with current LED settings from system
    led_pattern_type_e current_pattern;
    uint32_t current_param1, current_param2;
    esp_err_t ret = led_ctrl_get_pattern(&current_pattern, &current_param1, &current_param2);
    if (ret == ESP_OK) {
        // Update GUI with actual current settings
        gui->selected_pattern = current_pattern;
        lv_roller_set_selected(gui->pattern_roller, gui->selected_pattern, LV_ANIM_OFF);

        // Update parameter buffers and text fields
        snprintf(gui->param1_buffer, sizeof(gui->param1_buffer), "%lu", current_param1);
        snprintf(gui->param2_buffer, sizeof(gui->param2_buffer), "%lu", current_param2);
        lv_textarea_set_text(gui->param1_textfield, gui->param1_buffer);
        lv_textarea_set_text(gui->param2_textfield, gui->param2_buffer);

        ESP_LOGI(TAG, "Loaded current LED pattern: %s, param1: %lu, param2: %lu",
                 led_pattern_names[current_pattern], current_param1, current_param2);
    } else {
        ESP_LOGW(TAG, "Failed to get current LED pattern settings: %s, using defaults", esp_err_to_name(ret));
    }

    // Show the LED pattern settings container
    lv_obj_remove_flag(gui->container, LV_OBJ_FLAG_HIDDEN);
    ESP_LOGI(TAG, "Showed LED pattern settings container");

    return ESP_OK;
}

static esp_err_t post_led_pattern_settings_gui(struct menu_item *self)
{
    ESP_LOGD(TAG, "Post LED pattern settings GUI cleanup");

    if (!self || !self->user_ctx) {
        return ESP_OK;
    }

    led_pattern_settings_gui_t *gui = (led_pattern_settings_gui_t *)self->user_ctx;

    // Hide the LED pattern settings container
    lv_obj_add_flag(gui->container, LV_OBJ_FLAG_HIDDEN);
    ESP_LOGI(TAG, "Hidden LED pattern settings container");

    return ESP_OK;
}

static void update_led_pattern_focus_style(led_pattern_settings_gui_t *gui)
{
    if (!gui) return;

    // Reset all borders and styling
    lv_obj_set_style_border_width(gui->pattern_section, 0, 0);
    lv_obj_set_style_border_width(gui->param1_textfield, 0, 0);
    lv_obj_set_style_border_width(gui->param2_textfield, 0, 0);

    // Reset roller to default unfocused state
    lv_obj_set_style_bg_color(gui->pattern_roller, lv_color_hex(0x333333), 0);
    lv_obj_set_style_text_color(gui->pattern_roller, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(gui->pattern_roller, LV_OPA_COVER, 0);

    // Ensure selected part styling is always clean
    lv_obj_set_style_bg_opa(gui->pattern_roller, LV_OPA_TRANSP, LV_PART_SELECTED);
    lv_obj_set_style_border_width(gui->pattern_roller, 0, LV_PART_SELECTED);
    lv_obj_set_style_outline_width(gui->pattern_roller, 0, LV_PART_SELECTED);

    // Set focused widget styling
    switch (gui->current_focus) {
        case WIDGET_FOCUS_LED_PATTERN:
            // Apply the same focus style as textareas but to the pattern section container
            lv_obj_set_style_border_width(gui->pattern_section, 2, 0);
            lv_obj_set_style_border_color(gui->pattern_section, lv_color_hex(0x00AAFF), 0);
            break;
        case WIDGET_FOCUS_LED_PARAM1:
            lv_obj_set_style_border_width(gui->param1_textfield, 2, 0);
            lv_obj_set_style_border_color(gui->param1_textfield, lv_color_hex(0x00AAFF), 0);
            break;
        case WIDGET_FOCUS_LED_PARAM2:
            lv_obj_set_style_border_width(gui->param2_textfield, 2, 0);
            lv_obj_set_style_border_color(gui->param2_textfield, lv_color_hex(0x00AAFF), 0);
            break;
        default:
            break;
    }

    // Update cursor display for text fields
    update_led_pattern_cursor_display(gui);
}

static void update_led_pattern_cursor_display(led_pattern_settings_gui_t *gui)
{
    if (!gui) return;

    // Update param1 field display with cursor
    if (gui->current_focus == WIDGET_FOCUS_LED_PARAM1) {
        char display_text[16];
        int len = strlen(gui->param1_buffer);
        int pos = gui->cursor_pos_param1;

        if (pos >= len) {
            snprintf(display_text, sizeof(display_text), "%s|", gui->param1_buffer);
        } else {
            char before[8], after[8];
            strncpy(before, gui->param1_buffer, pos);
            before[pos] = '\0';
            strcpy(after, gui->param1_buffer + pos);
            snprintf(display_text, sizeof(display_text), "%s|%s", before, after);
        }
        lv_textarea_set_text(gui->param1_textfield, display_text);
    } else {
        lv_textarea_set_text(gui->param1_textfield, gui->param1_buffer);
    }

    // Update param2 field display with cursor
    if (gui->current_focus == WIDGET_FOCUS_LED_PARAM2) {
        char display_text[16];
        int len = strlen(gui->param2_buffer);
        int pos = gui->cursor_pos_param2;

        if (pos >= len) {
            snprintf(display_text, sizeof(display_text), "%s|", gui->param2_buffer);
        } else {
            char before[8], after[8];
            strncpy(before, gui->param2_buffer, pos);
            before[pos] = '\0';
            strcpy(after, gui->param2_buffer + pos);
            snprintf(display_text, sizeof(display_text), "%s|%s", before, after);
        }
        lv_textarea_set_text(gui->param2_textfield, display_text);
    } else {
        lv_textarea_set_text(gui->param2_textfield, gui->param2_buffer);
    }
}

static bool led_pattern_settings_handle_input_key(void *user_ctx, input_event_e input_event, char key_code)
{
    led_pattern_settings_gui_t *gui = (led_pattern_settings_gui_t *)user_ctx;
    if (!gui) return false;

    switch (input_event) {
        case INPUT_EVENT_TAB:
            // Navigate between widgets
            gui->current_focus = (led_pattern_widget_focus_e)((gui->current_focus + 1) % WIDGET_FOCUS_COUNT_PATTERN);

            // Set cursor to end of text field when switching to it
            if (gui->current_focus == WIDGET_FOCUS_LED_PARAM1) {
                gui->cursor_pos_param1 = strlen(gui->param1_buffer);
            } else if (gui->current_focus == WIDGET_FOCUS_LED_PARAM2) {
                gui->cursor_pos_param2 = strlen(gui->param2_buffer);
            }

            update_led_pattern_focus_style(gui);
            return true;

        case INPUT_EVENT_RIGHT_ARROW:
            if (gui->current_focus == WIDGET_FOCUS_LED_PARAM1) {
                // Move cursor right in param1 field
                if (gui->cursor_pos_param1 < strlen(gui->param1_buffer)) {
                    gui->cursor_pos_param1++;
                    update_led_pattern_cursor_display(gui);
                }
            } else if (gui->current_focus == WIDGET_FOCUS_LED_PARAM2) {
                // Move cursor right in param2 field
                if (gui->cursor_pos_param2 < strlen(gui->param2_buffer)) {
                    gui->cursor_pos_param2++;
                    update_led_pattern_cursor_display(gui);
                }
            }
            return true;

        case INPUT_EVENT_LEFT_ARROW:
            if (gui->current_focus == WIDGET_FOCUS_LED_PARAM1) {
                // Move cursor left in param1 field
                if (gui->cursor_pos_param1 > 0) {
                    gui->cursor_pos_param1--;
                    update_led_pattern_cursor_display(gui);
                }
            } else if (gui->current_focus == WIDGET_FOCUS_LED_PARAM2) {
                // Move cursor left in param2 field
                if (gui->cursor_pos_param2 > 0) {
                    gui->cursor_pos_param2--;
                    update_led_pattern_cursor_display(gui);
                }
            }
            return true;

        case INPUT_EVENT_UP_ARROW:
            if (gui->current_focus == WIDGET_FOCUS_LED_PATTERN) {
                // Navigate up in pattern roller (same as left arrow)
                gui->selected_pattern = (gui->selected_pattern + LED_PATTERN_COUNT - 1) % LED_PATTERN_COUNT;
                lv_roller_set_selected(gui->pattern_roller, gui->selected_pattern, LV_ANIM_OFF);
            }
            // For text fields, up arrow doesn't do anything specific
            return true;

        case INPUT_EVENT_DOWN_ARROW:
            if (gui->current_focus == WIDGET_FOCUS_LED_PATTERN) {
                // Navigate down in pattern roller (same as right arrow)
                gui->selected_pattern = (gui->selected_pattern + 1) % LED_PATTERN_COUNT;
                lv_roller_set_selected(gui->pattern_roller, gui->selected_pattern, LV_ANIM_OFF);
            }
            // For text fields, down arrow doesn't do anything specific
            return true;

        case INPUT_EVENT_BACKSPACE:
            if (gui->current_focus == WIDGET_FOCUS_LED_PARAM1) {
                // Delete character in param1 field
                int len = strlen(gui->param1_buffer);
                if (gui->cursor_pos_param1 > 0 && len > 0) {
                    memmove(&gui->param1_buffer[gui->cursor_pos_param1 - 1],
                           &gui->param1_buffer[gui->cursor_pos_param1],
                           len - gui->cursor_pos_param1 + 1);
                    gui->cursor_pos_param1--;
                    update_led_pattern_cursor_display(gui);
                }
            } else if (gui->current_focus == WIDGET_FOCUS_LED_PARAM2) {
                // Delete character in param2 field
                int len = strlen(gui->param2_buffer);
                if (gui->cursor_pos_param2 > 0 && len > 0) {
                    memmove(&gui->param2_buffer[gui->cursor_pos_param2 - 1],
                           &gui->param2_buffer[gui->cursor_pos_param2],
                           len - gui->cursor_pos_param2 + 1);
                    gui->cursor_pos_param2--;
                    update_led_pattern_cursor_display(gui);
                }
            }
            return true;

        case INPUT_EVENT_KEYCODE:
            // Handle numeric input for parameter fields
            if (key_code >= '0' && key_code <= '9') {
                if (gui->current_focus == WIDGET_FOCUS_LED_PARAM1) {
                    int len = strlen(gui->param1_buffer);
                    if (len < 7 && gui->cursor_pos_param1 <= len) { // Leave space for cursor
                        memmove(&gui->param1_buffer[gui->cursor_pos_param1 + 1],
                               &gui->param1_buffer[gui->cursor_pos_param1],
                               len - gui->cursor_pos_param1 + 1);
                        gui->param1_buffer[gui->cursor_pos_param1] = key_code;
                        gui->cursor_pos_param1++;
                        update_led_pattern_cursor_display(gui);
                    }
                } else if (gui->current_focus == WIDGET_FOCUS_LED_PARAM2) {
                    int len = strlen(gui->param2_buffer);
                    if (len < 7 && gui->cursor_pos_param2 <= len) { // Leave space for cursor
                        memmove(&gui->param2_buffer[gui->cursor_pos_param2 + 1],
                               &gui->param2_buffer[gui->cursor_pos_param2],
                               len - gui->cursor_pos_param2 + 1);
                        gui->param2_buffer[gui->cursor_pos_param2] = key_code;
                        gui->cursor_pos_param2++;
                        update_led_pattern_cursor_display(gui);
                    }
                }
                return true;
            }
            break;

        default:
            break;
    }

    return false;
}

// Public interface functions for LED pattern settings
esp_err_t keyboard_gui_prepare_led_pattern_settings(struct menu_item *self)
{
    return prepare_led_pattern_settings_gui(self);
}

esp_err_t keyboard_gui_post_led_pattern_settings(struct menu_item *self)
{
    return post_led_pattern_settings_gui(self);
}

esp_err_t keyboard_gui_led_pattern_settings_action(void *user_ctx)
{
    ESP_LOGI(TAG, "LED pattern settings action triggered");

    if (!user_ctx) {
        ESP_LOGE(TAG, "Invalid LED pattern settings user context");
        return ESP_ERR_INVALID_ARG;
    }

    led_pattern_settings_gui_t *gui = (led_pattern_settings_gui_t *)user_ctx;

    // Apply LED pattern settings
    int param1_val = atoi(gui->param1_buffer);
    int param2_val = atoi(gui->param2_buffer);

    ESP_LOGI(TAG, "Applying LED pattern: %s (index: %d), param1: %d, param2: %d",
             led_pattern_names[gui->selected_pattern], gui->selected_pattern, param1_val, param2_val);

    // Apply LED pattern settings to the actual LED system
    esp_err_t ret = led_ctrl_set_pattern((led_pattern_type_e)gui->selected_pattern, param1_val, param2_val);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set LED pattern: %s", esp_err_to_name(ret));
        return ret;
    } else {
        ESP_LOGI(TAG, "LED pattern applied successfully");
    }

    return ESP_OK;
}

// Public interface function for LED pattern settings input handling
bool keyboard_gui_led_pattern_settings_handle_input(void *user_ctx, input_event_e input_event, char key_code)
{
    // The user_ctx is actually the GUI context directly, not a menu_item
    led_pattern_settings_gui_t *gui = (led_pattern_settings_gui_t *)user_ctx;

    if (!gui) {
        ESP_LOGE(TAG, "GUI context is NULL");
        return false;
    }

    return led_pattern_settings_handle_input_key(gui, input_event, key_code);
}

// Helper function to initialize focus styling after all functions are defined
static void initialize_led_pattern_focus_styling(led_pattern_settings_gui_t *gui)
{
    if (gui) {
        update_led_pattern_focus_style(gui);
    }
}
