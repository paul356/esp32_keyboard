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
#include "esp_wifi.h"
#include "lvgl.h"
#include "display_hardware_info.h"
#include "menu_state_machine.h"
#include "keyboard_gui_widgets.h"
#include "menu_icons.h"
#include "function_control.h"

#define TAG "gui_wifi"

#define WIFI_SSID_BUF_SIZE    64
#define WIFI_PASSWD_BUF_SIZE  64

// WiFi settings GUI focus states
typedef enum {
    WIDGET_FOCUS_WIFI_MODE = 0,   // Mode selector focus
    WIDGET_FOCUS_WIFI_SSID = 1,   // SSID text field focus
    WIDGET_FOCUS_WIFI_PASSWD = 2, // Password text field focus
    WIDGET_FOCUS_COUNT = 3        // Total number of focusable elements
} widget_focus_e;

// Number of usable WiFi modes (STA and AP)
#define WIFI_MODE_COUNT 2

// GUI context structure for WiFi functionality
typedef struct {
    lv_obj_t *container;
    lv_obj_t *wifi_icon;            // WiFi icon
    lv_obj_t *status_label;         // Label showing "Open" or "Close"
} wifi_toggle_gui_t;

// WiFi settings GUI context structure
typedef struct {
    lv_obj_t *container;
    lv_obj_t *mode_selector;        // Button matrix for wifi mode
    lv_obj_t *ssid_textfield;       // Text field for SSID
    lv_obj_t *password_textfield;   // Text field for password
    lv_obj_t *mode_label;           // Label for mode selector
    lv_obj_t *ssid_label;           // Label for SSID field
    lv_obj_t *password_label;       // Label for password field
    widget_focus_e current_focus; // Current focus state (mode, SSID, or password)
    int cursor_pos_ssid;            // Cursor position in SSID field
    int cursor_pos_password;        // Cursor position in password field
    char ssid_buffer[WIFI_SSID_BUF_SIZE];      // SSID input buffer
    char password_buffer[WIFI_PASSWD_BUF_SIZE]; // Password input buffer
    int selected_mode;              // 0=STA, 1=AP
} wifi_settings_gui_t;

// Static function declarations
static wifi_toggle_gui_t* create_wifi_toggle_gui(void);
static esp_err_t prepare_wifi_toggle_gui(struct menu_item *self);
static esp_err_t post_wifi_toggle_gui(struct menu_item *self);
static wifi_settings_gui_t* create_wifi_settings_gui(void);
static esp_err_t prepare_wifi_settings_gui(struct menu_item *self);
static esp_err_t post_wifi_settings_gui(struct menu_item *self);
static bool wifi_settings_handle_input_key(void *user_ctx, input_event_e input_event, char key_code);
static void update_focus_style(wifi_settings_gui_t *gui);
static void update_cursor_display(wifi_settings_gui_t *gui);

// Helper function to convert wifi_mode_t to mode selector index
static int wifi_mode_to_selector_index(wifi_mode_t mode)
{
    switch (mode) {
        case WIFI_MODE_STA:
            return 0;
        case WIFI_MODE_AP:
            return 1;
        default:
            return 0; // Default to STA
    }
}

// Helper function to convert mode selector index to wifi_mode_t
static wifi_mode_t selector_index_to_wifi_mode(int index)
{
    switch (index) {
        case 0:
            return WIFI_MODE_STA;
        case 1:
            return WIFI_MODE_AP;
        default:
            return WIFI_MODE_STA; // Default to STA
    }
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
    if (is_wifi_enabled()) {
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
    if (is_wifi_enabled()) {
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

static wifi_settings_gui_t* create_wifi_settings_gui(void)
{
    wifi_settings_gui_t *gui = malloc(sizeof(wifi_settings_gui_t));
    if (!gui) {
        ESP_LOGE(TAG, "Failed to allocate wifi_settings GUI");
        return NULL;
    }

    // Initialize GUI state
    gui->current_focus = WIDGET_FOCUS_WIFI_MODE;
    gui->cursor_pos_ssid = 0;
    gui->cursor_pos_password = 0;
    gui->selected_mode = 0; // STA mode by default
    strcpy(gui->ssid_buffer, "");
    strcpy(gui->password_buffer, "");

    // Create main container
    gui->container = lv_obj_create(lv_screen_active());
    lv_obj_set_size(gui->container, LCD_WIDTH, LCD_HEIGHT);
    lv_obj_set_pos(gui->container, 0, 0);
    lv_obj_set_style_bg_color(gui->container, lv_color_black(), 0);
    lv_obj_set_style_border_width(gui->container, 0, 0);
    lv_obj_set_style_pad_all(gui->container, 5, 0);
    lv_obj_clear_flag(gui->container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(gui->container, LV_OBJ_FLAG_HIDDEN);

    // Set horizontal layout - mode selector on left, text fields on right
    lv_obj_set_flex_flow(gui->container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(gui->container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // Left section - Mode selector (vertical layout)
    lv_obj_t *left_section = lv_obj_create(gui->container);
    lv_obj_set_size(left_section, 130, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(left_section, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(left_section, 0, 0);
    lv_obj_set_style_pad_all(left_section, 3, 0);
    lv_obj_set_flex_flow(left_section, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(left_section, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    gui->mode_label = lv_label_create(left_section);
    lv_label_set_text(gui->mode_label, "Mode:");
    lv_obj_set_style_text_color(gui->mode_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(gui->mode_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_margin_bottom(gui->mode_label, 3, 0);

    gui->mode_selector = lv_buttonmatrix_create(left_section);
    // Use wifi_mode_to_str to get string representation for the two modes
    static const char * mode_map[] = {NULL, NULL, ""};
    mode_map[0] = wifi_mode_to_str(WIFI_MODE_STA);
    mode_map[1] = wifi_mode_to_str(WIFI_MODE_AP);
    lv_buttonmatrix_set_map(gui->mode_selector, mode_map);
    lv_buttonmatrix_set_button_ctrl_all(gui->mode_selector, LV_BUTTONMATRIX_CTRL_CHECKABLE);
    lv_buttonmatrix_set_one_checked(gui->mode_selector, true);
    // Set the initial checked button to mode 0 (STA)
    lv_buttonmatrix_set_button_ctrl(gui->mode_selector, 0, LV_BUTTONMATRIX_CTRL_CHECKED);
    lv_obj_set_size(gui->mode_selector, 120, 26);

    // Style the button matrix background
    lv_obj_set_style_bg_color(gui->mode_selector, lv_color_hex(0x333333), 0);
    lv_obj_set_style_text_color(gui->mode_selector, lv_color_white(), 0);
    lv_obj_set_style_pad_all(gui->mode_selector, 1, 0);
    lv_obj_set_style_pad_gap(gui->mode_selector, 2, 0);

    // Style unselected buttons (default state)
    lv_obj_set_style_bg_color(gui->mode_selector, lv_color_hex(0x555555), LV_PART_ITEMS);
    lv_obj_set_style_text_color(gui->mode_selector, lv_color_white(), LV_PART_ITEMS);
    lv_obj_set_style_border_width(gui->mode_selector, 1, LV_PART_ITEMS);
    lv_obj_set_style_border_color(gui->mode_selector, lv_color_hex(0x777777), LV_PART_ITEMS);

    // Style selected/checked buttons
    lv_obj_set_style_bg_color(gui->mode_selector, lv_color_hex(0x0080FF), LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_text_color(gui->mode_selector, lv_color_white(), LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_border_width(gui->mode_selector, 2, LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_border_color(gui->mode_selector, lv_color_hex(0x00AAFF), LV_PART_ITEMS | LV_STATE_CHECKED);

    // Right section - Text fields (vertical layout)
    lv_obj_t *right_section = lv_obj_create(gui->container);
    lv_obj_set_size(right_section, LCD_WIDTH - 150, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(right_section, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(right_section, 0, 0);
    lv_obj_set_style_pad_all(right_section, 3, 0);
    lv_obj_set_flex_flow(right_section, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(right_section, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

    // SSID field
    lv_obj_t *ssid_row = lv_obj_create(right_section);
    lv_obj_set_size(ssid_row, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(ssid_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(ssid_row, 0, 0);
    lv_obj_set_style_pad_all(ssid_row, 1, 0);
    lv_obj_set_style_margin_bottom(ssid_row, 2, 0);
    lv_obj_set_flex_flow(ssid_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(ssid_row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(ssid_row, LV_OBJ_FLAG_SCROLLABLE);

    gui->ssid_label = lv_label_create(ssid_row);
    lv_label_set_text(gui->ssid_label, "SSID:");
    lv_obj_set_style_text_color(gui->ssid_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(gui->ssid_label, &lv_font_montserrat_12, 0);
    lv_obj_set_width(gui->ssid_label, 35);

    gui->ssid_textfield = lv_textarea_create(ssid_row);
    lv_textarea_set_one_line(gui->ssid_textfield, true);
    lv_textarea_set_placeholder_text(gui->ssid_textfield, "WiFi SSID");
    lv_obj_set_width(gui->ssid_textfield, 90);
    lv_obj_set_height(gui->ssid_textfield, 26);
    lv_obj_set_style_text_color(gui->ssid_textfield, lv_color_white(), 0);
    lv_obj_set_style_bg_color(gui->ssid_textfield, lv_color_hex(0x333333), 0);
    lv_obj_set_style_text_font(gui->ssid_textfield, &lv_font_montserrat_10, 0);
    lv_obj_clear_flag(gui->ssid_textfield, LV_OBJ_FLAG_SCROLLABLE);

    // Password field
    lv_obj_t *password_row = lv_obj_create(right_section);
    lv_obj_set_size(password_row, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(password_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(password_row, 0, 0);
    lv_obj_set_style_pad_all(password_row, 1, 0);
    lv_obj_set_flex_flow(password_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(password_row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(password_row, LV_OBJ_FLAG_SCROLLABLE);

    gui->password_label = lv_label_create(password_row);
    lv_label_set_text(gui->password_label, "Pass:");
    lv_obj_set_style_text_color(gui->password_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(gui->password_label, &lv_font_montserrat_12, 0);
    lv_obj_set_width(gui->password_label, 35);

    gui->password_textfield = lv_textarea_create(password_row);
    lv_textarea_set_one_line(gui->password_textfield, true);
    lv_textarea_set_placeholder_text(gui->password_textfield, "Password");
    // lv_textarea_set_password_mode(gui->password_textfield, true); // Disabled: using custom display
    lv_obj_set_width(gui->password_textfield, 90);
    lv_obj_set_height(gui->password_textfield, 26);
    lv_obj_set_style_text_color(gui->password_textfield, lv_color_white(), 0);
    lv_obj_set_style_bg_color(gui->password_textfield, lv_color_hex(0x333333), 0);
    lv_obj_set_style_text_font(gui->password_textfield, &lv_font_montserrat_10, 0);
    lv_obj_clear_flag(gui->password_textfield, LV_OBJ_FLAG_SCROLLABLE);

    // Set initial focus styling
    update_focus_style(gui);

    return gui;
}

static void update_focus_style(wifi_settings_gui_t *gui)
{
    if (!gui) return;

    // Reset all borders
    lv_obj_set_style_border_width(gui->mode_selector, 0, 0);
    lv_obj_set_style_border_width(gui->ssid_textfield, 0, 0);
    lv_obj_set_style_border_width(gui->password_textfield, 0, 0);

    // Set focused widget border
    lv_obj_t *focused_widget = NULL;
    switch (gui->current_focus) {
        case WIDGET_FOCUS_WIFI_MODE:
            focused_widget = gui->mode_selector;
            break;
        case WIDGET_FOCUS_WIFI_SSID:
            focused_widget = gui->ssid_textfield;
            break;
        case WIDGET_FOCUS_WIFI_PASSWD:
            focused_widget = gui->password_textfield;
            break;
        default:
            break;
    }

    if (focused_widget) {
        lv_obj_set_style_border_width(focused_widget, 2, 0);
        lv_obj_set_style_border_color(focused_widget, lv_color_hex(0x0080FF), 0);
    }

    update_cursor_display(gui);
}

static void update_cursor_display(wifi_settings_gui_t *gui)
{
    if (!gui) return;

    // Update text field contents with current buffer and cursor
    if (gui->current_focus == WIDGET_FOCUS_WIFI_SSID) {
        // SSID field - show cursor
        char display_text[128]; // Increased buffer size to prevent truncation warnings
        int pos = gui->cursor_pos_ssid;
        int len = strlen(gui->ssid_buffer);

        if (pos >= len) {
            snprintf(display_text, sizeof(display_text), "%s|", gui->ssid_buffer);
        } else {
            char before[64], after[64];
            strncpy(before, gui->ssid_buffer, pos);
            before[pos] = '\0';
            strcpy(after, gui->ssid_buffer + pos);
            snprintf(display_text, sizeof(display_text), "%s|%s", before, after);
        }
        lv_textarea_set_text(gui->ssid_textfield, display_text);
    } else {
        lv_textarea_set_text(gui->ssid_textfield, gui->ssid_buffer);
    }

    if (gui->current_focus == WIDGET_FOCUS_WIFI_PASSWD) {
        // Password field - show cursor with asterisks
        char display_text[128];
        int pos = gui->cursor_pos_password;
        int len = strlen(gui->password_buffer);

        char asterisks[64];
        for (int i = 0; i < len; i++) {
            asterisks[i] = '*';
        }
        asterisks[len] = '\0';

        if (pos >= len) {
            snprintf(display_text, sizeof(display_text), "%s|", asterisks);
        } else {
            char before[64], after[64];
            strncpy(before, asterisks, pos);
            before[pos] = '\0';
            strcpy(after, asterisks + pos);
            snprintf(display_text, sizeof(display_text), "%s|%s", before, after);
        }
        lv_textarea_set_text(gui->password_textfield, display_text);
    } else {
        // Show asterisks without cursor
        char asterisks[64];
        int len = strlen(gui->password_buffer);
        for (int i = 0; i < len; i++) {
            asterisks[i] = '*';
        }
        asterisks[len] = '\0';
        lv_textarea_set_text(gui->password_textfield, asterisks);
    }
}

static bool wifi_settings_handle_input_key(void *user_ctx, input_event_e input_event, char key_code)
{
    wifi_settings_gui_t *gui = (wifi_settings_gui_t *)user_ctx;
    if (!gui) return false;

    switch (input_event) {
        case INPUT_EVENT_TAB:
            // Navigate between widgets
            gui->current_focus = (widget_focus_e)((gui->current_focus + 1) % WIDGET_FOCUS_COUNT);
            update_focus_style(gui);
            return true;

        case INPUT_EVENT_RIGHT_ARROW:
            if (gui->current_focus == WIDGET_FOCUS_WIFI_MODE) {
                // Navigate in mode selector
                gui->selected_mode = (gui->selected_mode + 1) % WIFI_MODE_COUNT;
                lv_buttonmatrix_clear_button_ctrl_all(gui->mode_selector, LV_BUTTONMATRIX_CTRL_CHECKED);
                lv_buttonmatrix_set_button_ctrl(gui->mode_selector, gui->selected_mode, LV_BUTTONMATRIX_CTRL_CHECKED);
            } else if (gui->current_focus == WIDGET_FOCUS_WIFI_SSID) {
                // Move cursor right in SSID field
                if (gui->cursor_pos_ssid < strlen(gui->ssid_buffer)) {
                    gui->cursor_pos_ssid++;
                    update_cursor_display(gui);
                }
            } else if (gui->current_focus == WIDGET_FOCUS_WIFI_PASSWD) {
                // Move cursor right in password field
                if (gui->cursor_pos_password < strlen(gui->password_buffer)) {
                    gui->cursor_pos_password++;
                    update_cursor_display(gui);
                }
            }
            return true;

        case INPUT_EVENT_LEFT_ARROW:
            if (gui->current_focus == WIDGET_FOCUS_WIFI_MODE) {
                // Navigate in mode selector
                gui->selected_mode = (gui->selected_mode + WIFI_MODE_COUNT - 1) % WIFI_MODE_COUNT; // Proper left navigation
                lv_buttonmatrix_clear_button_ctrl_all(gui->mode_selector, LV_BUTTONMATRIX_CTRL_CHECKED);
                lv_buttonmatrix_set_button_ctrl(gui->mode_selector, gui->selected_mode, LV_BUTTONMATRIX_CTRL_CHECKED);
            } else if (gui->current_focus == WIDGET_FOCUS_WIFI_SSID) {
                // Move cursor left in SSID field
                if (gui->cursor_pos_ssid > 0) {
                    gui->cursor_pos_ssid--;
                    update_cursor_display(gui);
                }
            } else if (gui->current_focus == WIDGET_FOCUS_WIFI_PASSWD) {
                // Move cursor left in password field
                if (gui->cursor_pos_password > 0) {
                    gui->cursor_pos_password--;
                    update_cursor_display(gui);
                }
            }
            return true;

        case INPUT_EVENT_BACKSPACE:
            if (gui->current_focus == WIDGET_FOCUS_WIFI_SSID) {
                // Delete character in SSID field
                int len = strlen(gui->ssid_buffer);
                if (gui->cursor_pos_ssid > 0 && len > 0) {
                    memmove(&gui->ssid_buffer[gui->cursor_pos_ssid - 1],
                           &gui->ssid_buffer[gui->cursor_pos_ssid],
                           len - gui->cursor_pos_ssid + 1);
                    gui->cursor_pos_ssid--;
                    update_cursor_display(gui);
                }
            } else if (gui->current_focus == WIDGET_FOCUS_WIFI_PASSWD) {
                // Delete character in password field
                int len = strlen(gui->password_buffer);
                if (gui->cursor_pos_password > 0 && len > 0) {
                    memmove(&gui->password_buffer[gui->cursor_pos_password - 1],
                           &gui->password_buffer[gui->cursor_pos_password],
                           len - gui->cursor_pos_password + 1);
                    gui->cursor_pos_password--;
                    update_cursor_display(gui);
                }
            }
            return true;

        case INPUT_EVENT_KEYCODE:
            // Handle alphanumeric input
            if (key_code >= 32 && key_code <= 126) { // Printable ASCII
                if (gui->current_focus == WIDGET_FOCUS_WIFI_SSID) {
                    // Insert character in SSID field
                    int len = strlen(gui->ssid_buffer);
                    if (len < WIFI_SSID_BUF_SIZE - 1) {
                        memmove(&gui->ssid_buffer[gui->cursor_pos_ssid + 1],
                               &gui->ssid_buffer[gui->cursor_pos_ssid],
                               len - gui->cursor_pos_ssid + 1);
                        gui->ssid_buffer[gui->cursor_pos_ssid] = key_code;
                        gui->cursor_pos_ssid++;
                        update_cursor_display(gui);
                    }
                } else if (gui->current_focus == WIDGET_FOCUS_WIFI_PASSWD) {
                    // Insert character in password field
                    int len = strlen(gui->password_buffer);
                    if (len < WIFI_PASSWD_BUF_SIZE - 1) {
                        memmove(&gui->password_buffer[gui->cursor_pos_password + 1],
                               &gui->password_buffer[gui->cursor_pos_password],
                               len - gui->cursor_pos_password + 1);
                        gui->password_buffer[gui->cursor_pos_password] = key_code;
                        gui->cursor_pos_password++;
                        update_cursor_display(gui);
                    }
                }
            }
            return true;

        default:
            break;
    }

    return false;
}

static esp_err_t prepare_wifi_settings_gui(struct menu_item *self)
{
    ESP_LOGI(TAG, "Preparing WiFi settings GUI");

    if (!self) {
        return ESP_ERR_INVALID_ARG;
    }

    // Create WiFi settings GUI if not already created
    if (!self->user_ctx) {
        self->user_ctx = create_wifi_settings_gui();
        if (!self->user_ctx) {
            ESP_LOGE(TAG, "Failed to create WiFi settings GUI");
            return ESP_ERR_NO_MEM;
        }
        ESP_LOGI(TAG, "Created new WiFi settings GUI");
    }

    wifi_settings_gui_t *gui = (wifi_settings_gui_t *)self->user_ctx;

    // Load current WiFi settings if available
    wifi_mode_t current_mode = get_wifi_mode();
    const char* current_ssid = get_wifi_ssid();

    // Set mode based on current WiFi mode
    int mode_index = wifi_mode_to_selector_index(current_mode);

    gui->selected_mode = mode_index;
    lv_buttonmatrix_clear_button_ctrl_all(gui->mode_selector, LV_BUTTONMATRIX_CTRL_CHECKED);
    lv_buttonmatrix_set_button_ctrl(gui->mode_selector, mode_index, LV_BUTTONMATRIX_CTRL_CHECKED);

    // Load current SSID if available
    if (current_ssid && strlen(current_ssid) > 0) {
        strncpy(gui->ssid_buffer, current_ssid, sizeof(gui->ssid_buffer) - 1);
        gui->ssid_buffer[sizeof(gui->ssid_buffer) - 1] = '\0';
        gui->cursor_pos_ssid = strlen(gui->ssid_buffer);
    } else {
        strcpy(gui->ssid_buffer, "");
        gui->cursor_pos_ssid = 0;
    }

    // Load current password
    const char* current_passwd = get_wifi_passwd();
    if (current_passwd && strlen(current_passwd) > 0) {
        strncpy(gui->password_buffer, current_passwd, sizeof(gui->password_buffer) - 1);
        gui->password_buffer[sizeof(gui->password_buffer) - 1] = '\0';
        gui->cursor_pos_password = strlen(gui->password_buffer);
    } else {
        strcpy(gui->password_buffer, "");
        gui->cursor_pos_password = 0;
    }

    // Update display
    update_cursor_display(gui);

    // Show the WiFi settings container
    lv_obj_remove_flag(gui->container, LV_OBJ_FLAG_HIDDEN);
    ESP_LOGI(TAG, "Showed WiFi settings container");

    return ESP_OK;
}

static esp_err_t post_wifi_settings_gui(struct menu_item *self)
{
    ESP_LOGD(TAG, "Post WiFi settings GUI cleanup");

    if (!self || !self->user_ctx) {
        return ESP_OK;
    }

    wifi_settings_gui_t *gui = (wifi_settings_gui_t *)self->user_ctx;

    // Hide the WiFi settings container
    lv_obj_add_flag(gui->container, LV_OBJ_FLAG_HIDDEN);
    ESP_LOGI(TAG, "Hidden WiFi settings container");

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
    if (is_wifi_enabled()) {
        // WiFi is currently enabled, disable it
        ESP_LOGI(TAG, "Disabling WiFi");
        esp_err_t ret = update_wifi_switch(false);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to disable WiFi: %s", esp_err_to_name(ret));
            return ret;
        }

        // Update GUI to reflect disabled state
        lv_label_set_text(gui->status_label, "Open");
        lv_obj_set_style_image_recolor_opa(gui->wifi_icon, LV_OPA_50, 0);
        lv_obj_set_style_image_recolor(gui->wifi_icon, lv_color_hex(0x808080), 0);
    } else {
        // WiFi is currently disabled, enable it
        ESP_LOGI(TAG, "Enabling WiFi");
        esp_err_t ret = update_wifi_switch(true);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to enable WiFi: %s", esp_err_to_name(ret));
            return ret;
        }

        // Update GUI to reflect enabled state
        lv_label_set_text(gui->status_label, "Close");
        lv_obj_set_style_image_recolor_opa(gui->wifi_icon, LV_OPA_TRANSP, 0);
    }

    ESP_LOGI(TAG, "WiFi toggle action completed successfully");
    return ESP_OK;
}

// ============================================================================
// Public API functions for WiFi settings menu integration
// ============================================================================

esp_err_t keyboard_gui_prepare_wifi_settings(struct menu_item *self)
{
    return prepare_wifi_settings_gui(self);
}

esp_err_t keyboard_gui_post_wifi_settings(struct menu_item *self)
{
    return post_wifi_settings_gui(self);
}

bool keyboard_gui_wifi_settings_handle_input(void *user_ctx, input_event_e input_event, char key_code)
{
    return wifi_settings_handle_input_key(user_ctx, input_event, key_code);
}

esp_err_t keyboard_gui_wifi_settings_action(void *user_ctx)
{
    ESP_LOGI(TAG, "WiFi settings action triggered");

    if (!user_ctx) {
        ESP_LOGE(TAG, "Invalid WiFi settings user context");
        return ESP_ERR_INVALID_ARG;
    }

    wifi_settings_gui_t *gui = (wifi_settings_gui_t *)user_ctx;

    // Confirm - save settings
    ESP_LOGI(TAG, "WiFi settings confirmed: Mode=%s, SSID='%s'",
             wifi_mode_to_str(selector_index_to_wifi_mode(gui->selected_mode)),
             gui->ssid_buffer);

    // Convert selected mode index to wifi_mode_t
    wifi_mode_t new_mode = selector_index_to_wifi_mode(gui->selected_mode);

    // Apply WiFi settings
    esp_err_t ret = update_wifi_mode(new_mode, gui->ssid_buffer, gui->password_buffer);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "WiFi settings applied successfully");
    } else {
        ESP_LOGE(TAG, "Failed to apply WiFi settings: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "WiFi settings action completed successfully");
    return ESP_OK;
}
