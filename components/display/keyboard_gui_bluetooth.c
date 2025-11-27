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

#include "esp_err.h"
#include "esp_gap_ble_api.h"
#include "esp_log.h"
#include "function_control.h"
#include "hal_ble.h"
#include "keyboard_gui_widgets.h"
#include "display_hardware_info.h"
#include "lvgl.h"
#include "menu_icons.h"
#include "menu_state_machine.h"
#include "esp_hidd.h"
#include <stdlib.h>
#include <string.h>

#define TAG "gui_bluetooth"

// GUI context structures for Bluetooth functionality
typedef struct
{
    lv_obj_t *container;
    lv_obj_t *bluetooth_icon; // Bluetooth icon
    lv_obj_t *status_label;   // Label showing "Open" or "Close"
} bt_toggle_gui_t;

typedef struct
{
    lv_obj_t *container;
    lv_obj_t *bluetooth_pair_icon; // Bluetooth pair icon
    lv_obj_t *status_label;        // Label showing "Connect <ble_name> on"
    lv_obj_t *passkey_label;       // Label showing "Type passkey now: <passkey>"
    uint32_t last_timeout_ms;
    bool prompt_for_passkey;
    char input_buffer[16]; // Buffer to collect numeric input for PIN entry
    int input_length;      // Current length of input in buffer
    esp_bd_addr_t pair_bda;
} bt_pair_kb_gui_t;

typedef struct
{
    lv_obj_t *container;
    lv_obj_t *bluetooth_pair_icon; // Bluetooth phone pair icon
    lv_obj_t *status_label;        // Label showing "Connect <ble_name> on"
    uint32_t last_timeout_ms;
} bt_pair_admin_gui_t;

// Report target selector GUI context structure
typedef struct {
    lv_obj_t *container;
    lv_obj_t *target_selector;      // Button matrix for target selection (USB/BLE)
    lv_obj_t *conn_info_label;      // Label for connection info
    int selected_target;            // 0=USB, 1=BLE
    int ble_device_index;           // Current BLE device index (0-based)
    int ble_device_count;           // Total number of BLE devices
    uint16_t ble_conn_id;           // Current BLE connection ID (if TARGET_BLE)
    esp_bd_addr_t ble_bda;          // Current BLE device address (if TARGET_BLE)
    bool is_broadcast_mode;         // True if BLE is in broadcast mode
} report_target_gui_t;

// Static function declarations
static bt_toggle_gui_t *create_bt_toggle_gui(void);
static bt_pair_kb_gui_t *create_bt_pair_kb_gui(void);
static esp_err_t prepare_bt_toggle_gui(struct menu_item *self);
static esp_err_t post_bt_toggle_gui(struct menu_item *self);
static esp_err_t prepare_bt_pair_kb_gui(struct menu_item *self);
static esp_err_t post_bt_pair_kb_gui(struct menu_item *self);
static void update_passkey_display(bt_pair_kb_gui_t *gui);
static bool bt_pair_kb_handle_input_key(void *user_ctx, input_event_e input_event, char key_code);
static report_target_gui_t* create_report_target_gui(void);
static esp_err_t prepare_report_target_gui(struct menu_item *self);
static esp_err_t post_report_target_gui(struct menu_item *self);
static bool report_target_handle_input_key(void *user_ctx, input_event_e input_event, char key_code);
static void update_report_target_focus_style(report_target_gui_t *gui);
static void update_conn_info_display(report_target_gui_t *gui);
static void init_conn_info_display(report_target_gui_t *gui);

static bt_toggle_gui_t *create_bt_toggle_gui(void)
{
    bt_toggle_gui_t *gui = malloc(sizeof(bt_toggle_gui_t));
    if (!gui)
    {
        ESP_LOGE(TAG, "Failed to allocate bt_toggle GUI");
        return NULL;
    }

    // Create container for bt_toggle interface
    gui->container = lv_obj_create(lv_screen_active());
    lv_obj_set_size(gui->container, LCD_WIDTH, LCD_HEIGHT);
    lv_obj_set_pos(gui->container, 0, 0);
    lv_obj_set_style_bg_color(gui->container, lv_color_black(), 0);
    lv_obj_set_style_border_width(gui->container, 0, 0);
    lv_obj_set_style_pad_all(gui->container, 20, 0);

    // Disable scrollbars for bt_toggle container
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
    lv_obj_set_style_margin_right(gui->status_label, 20, 0); // Gap between label and icon

    // Bluetooth icon
    gui->bluetooth_icon = lv_image_create(gui->container);
    lv_image_set_src(gui->bluetooth_icon, &bluetooth_icon);

    // Set initial label text and icon appearance based on current Bluetooth state
    if (is_ble_enabled())
    {
        lv_label_set_text(gui->status_label, "Close");
        // Icon is colored normally when BT is enabled
        lv_obj_set_style_image_recolor_opa(gui->bluetooth_icon, LV_OPA_TRANSP, 0);
    }
    else
    {
        lv_label_set_text(gui->status_label, "Open");
        // Grey out the icon when BT is disabled
        lv_obj_set_style_image_recolor_opa(gui->bluetooth_icon, LV_OPA_50, 0);
        lv_obj_set_style_image_recolor(gui->bluetooth_icon, lv_color_hex(0x808080), 0);
    }

    return gui;
}

static bt_pair_kb_gui_t *create_bt_pair_kb_gui(void)
{
    bt_pair_kb_gui_t *gui = malloc(sizeof(bt_pair_kb_gui_t));
    if (!gui)
    {
        ESP_LOGE(TAG, "Failed to allocate memory for bt_pair_kb GUI");
        return NULL;
    }

    // Create container for bt_pair_kb interface
    gui->container = lv_obj_create(lv_screen_active());
    lv_obj_set_size(gui->container, LCD_WIDTH, LCD_HEIGHT);
    lv_obj_set_pos(gui->container, 0, 0);
    lv_obj_set_style_bg_color(gui->container, lv_color_black(), 0);
    lv_obj_set_style_border_width(gui->container, 0, 0);
    lv_obj_set_style_pad_all(gui->container, 8, 0);

    // Disable scrollbars for bt_pair_kb container
    lv_obj_clear_flag(gui->container, LV_OBJ_FLAG_SCROLLABLE);

    // Hide container initially - will be shown when prepare_gui_func is called
    lv_obj_add_flag(gui->container, LV_OBJ_FLAG_HIDDEN);

    // Set horizontal flex layout for the container - icon on left, labels on right
    lv_obj_set_flex_flow(gui->container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(gui->container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // Bluetooth pair icon on the left
    gui->bluetooth_pair_icon = lv_image_create(gui->container);
    lv_image_set_src(gui->bluetooth_pair_icon, &bluetooth_pc_pair);

    // Create a vertical container for the labels on the right side
    lv_obj_t *labels_container = lv_obj_create(gui->container);
    lv_obj_set_style_bg_opa(labels_container, LV_OPA_TRANSP, 0); // Transparent background
    lv_obj_set_style_border_width(labels_container, 0, 0); // No border
    lv_obj_set_style_pad_all(labels_container, 0, 0); // No padding
    lv_obj_set_flex_flow(labels_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(labels_container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_flex_grow(labels_container, 1); // Take remaining space

    // Status label at the top of labels container
    gui->status_label = lv_label_create(labels_container);
    lv_obj_set_style_text_color(gui->status_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(gui->status_label, &lv_font_montserrat_12, 0); // Reduced font size
    lv_obj_set_style_text_align(gui->status_label, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_style_margin_bottom(gui->status_label, 5, 0); // Increased gap for visibility
    lv_label_set_text(gui->status_label, "");

    // Passkey label below status label
    gui->passkey_label = lv_label_create(labels_container);
    lv_obj_set_style_text_color(gui->passkey_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(gui->passkey_label, &lv_font_montserrat_12, 0); // Increased font size to match status label
    lv_obj_set_style_text_align(gui->passkey_label, LV_TEXT_ALIGN_LEFT, 0);
    lv_label_set_text(gui->passkey_label, ""); // Start empty, will be updated when prompting

    // Initialize input buffer and flag
    memset(gui->input_buffer, 0, sizeof(gui->input_buffer));
    gui->input_length = 0;
    gui->prompt_for_passkey = false;

    return gui;
}

static esp_err_t prepare_bt_toggle_gui(struct menu_item *self)
{
    ESP_LOGI(TAG, "Preparing Bluetooth toggle GUI");

    if (!self)
    {
        return ESP_ERR_INVALID_ARG;
    }

    // Create Bluetooth toggle GUI if not already created
    if (!self->user_ctx)
    {
        self->user_ctx = create_bt_toggle_gui();
        if (!self->user_ctx)
        {
            ESP_LOGE(TAG, "Failed to create Bluetooth toggle GUI");
            return ESP_ERR_NO_MEM;
        }
        ESP_LOGI(TAG, "Created new Bluetooth toggle GUI");
    }

    bt_toggle_gui_t *gui = (bt_toggle_gui_t *)self->user_ctx;

    // Update label text and icon appearance based on current Bluetooth state
    if (is_ble_enabled())
    {
        lv_label_set_text(gui->status_label, "Close");
        // Icon is colored normally when BT is enabled
        lv_obj_set_style_image_recolor_opa(gui->bluetooth_icon, LV_OPA_TRANSP, 0);
    }
    else
    {
        lv_label_set_text(gui->status_label, "Open");
        // Grey out the icon when BT is disabled
        lv_obj_set_style_image_recolor_opa(gui->bluetooth_icon, LV_OPA_50, 0);
        lv_obj_set_style_image_recolor(gui->bluetooth_icon, lv_color_hex(0x808080), 0);
    }

    // Show the Bluetooth toggle container
    lv_obj_remove_flag(gui->container, LV_OBJ_FLAG_HIDDEN);
    ESP_LOGI(TAG, "Showed Bluetooth toggle container");

    return ESP_OK;
}

static esp_err_t post_bt_toggle_gui(struct menu_item *self)
{
    ESP_LOGD(TAG, "Post Bluetooth toggle GUI cleanup");

    if (!self || !self->user_ctx)
    {
        return ESP_OK;
    }

    bt_toggle_gui_t *gui = (bt_toggle_gui_t *)self->user_ctx;

    // Hide the Bluetooth toggle container
    lv_obj_add_flag(gui->container, LV_OBJ_FLAG_HIDDEN);
    ESP_LOGI(TAG, "Hidden Bluetooth toggle container");

    return ESP_OK;
}

static esp_err_t prepare_bt_pair_kb_gui(struct menu_item *self)
{
    if (!self)
    {
        ESP_LOGE(TAG, "Cannot prepare BT pair keyboard GUI: menu item is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Preparing BT pair keyboard GUI");

    // Create GUI if it doesn't exist
    if (!self->user_ctx)
    {
        self->user_ctx = create_bt_pair_kb_gui();
        if (!self->user_ctx)
        {
            ESP_LOGE(TAG, "Failed to create BT pair keyboard GUI");
            return ESP_ERR_NO_MEM;
        }
    }

    bt_pair_kb_gui_t *gui = (bt_pair_kb_gui_t *)self->user_ctx;

    // Reset input buffer when menu is prepared
    memset(gui->input_buffer, 0, sizeof(gui->input_buffer));
    gui->input_length = 0;
    ESP_LOGI(TAG, "Reset BT pair input buffer");

    // Update the label text with current BLE name
    char label_text[64];
    const char *ble_name = get_ble_name();
    snprintf(label_text, sizeof(label_text), "Keyboard Name: %s", ble_name ? ble_name : "Device");
    lv_label_set_text(gui->status_label, label_text);

    // Show the GUI
    lv_obj_clear_flag(gui->container, LV_OBJ_FLAG_HIDDEN);
    gui->last_timeout_ms = menu_state_get_timeout_ms(); // Store current timeout

    menu_state_set_timeout_ms(0); // Disable timeout while pairing

    ESP_LOGI(TAG, "BT pair keyboard GUI prepared and displayed");
    return ESP_OK;
}

static esp_err_t post_bt_pair_kb_gui(struct menu_item *self)
{
    if (!self || !self->user_ctx)
    {
        ESP_LOGE(TAG, "Cannot cleanup BT pair keyboard GUI: invalid context");
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Cleaning up BT pair keyboard GUI");

    bt_pair_kb_gui_t *gui = (bt_pair_kb_gui_t *)self->user_ctx;

    menu_state_set_timeout_ms(gui->last_timeout_ms); // Restore previous timeout

    // Reset passkey label content
    lv_label_set_text(gui->passkey_label, "");

    // Reset input state
    memset(gui->input_buffer, 0, sizeof(gui->input_buffer));
    gui->input_length = 0;
    gui->prompt_for_passkey = false;

    // Hide the GUI
    lv_obj_add_flag(gui->container, LV_OBJ_FLAG_HIDDEN);

    return ESP_OK;
}

// Helper function to update the passkey display
static void update_passkey_display(bt_pair_kb_gui_t *gui)
{
    if (gui->prompt_for_passkey)
    {
        char passkey_text[64];
        // Show the passkey prompt with current input
        snprintf(passkey_text, sizeof(passkey_text), "Passkey: %s", gui->input_buffer);
        lv_label_set_text(gui->passkey_label, passkey_text);
    }
    else
    {
        lv_label_set_text(gui->passkey_label, "");
    }
}

// Function to handle input for BT pair keyboard menu
static bool bt_pair_kb_handle_input_key(void *user_ctx, input_event_e input_event, char key_code)
{
    if (!user_ctx)
    {
        ESP_LOGE(TAG, "Invalid user context in bt_pair_kb_handle_input_key");
        return false;
    }

    bt_pair_kb_gui_t *gui = (bt_pair_kb_gui_t *)user_ctx;

    // Handle numeric keycode input for PIN entry
    if (input_event == INPUT_EVENT_KEYCODE)
    {
        // Only process input if we're prompting for passkey
        if (!gui->prompt_for_passkey)
        {
            return false; // Let other handlers process the input
        }

        // Check if it's a numeric key (KC_1 through KC_0)
        if (key_code >= '0' && key_code <= '9')
        {
            if (gui->input_length < sizeof(gui->input_buffer) - 1)
            {
                gui->input_buffer[gui->input_length++] = key_code;
                gui->input_buffer[gui->input_length] = '\0';

                // Update the passkey display
                update_passkey_display(gui);

                ESP_LOGI(TAG, "BT pair PIN digit entered: %c, current input: %s", key_code, gui->input_buffer);
                return true; // Consumed the input
            }
        }
        return false; // Let other keys be handled normally
    }

    // Handle backspace to delete last digit
    if (input_event == INPUT_EVENT_BACKSPACE)
    {
        // Only process backspace if we're prompting for passkey
        if (!gui->prompt_for_passkey)
        {
            return false; // Let other handlers process the input
        }

        if (gui->input_length > 0)
        {
            gui->input_buffer[--gui->input_length] = '\0';
        }

        // Update the passkey display
        update_passkey_display(gui);

        ESP_LOGI(TAG, "BT pair PIN backspace, current input: %s", gui->input_buffer);
        return true; // Always consume backspace in pairing mode
    }

    // Return false for unhandled events
    return false;
}

// ============================================================================
// Public API functions for menu integration
// ============================================================================

esp_err_t keyboard_gui_prepare_bt_toggle(struct menu_item *self)
{
    return prepare_bt_toggle_gui(self);
}

esp_err_t keyboard_gui_post_bt_toggle(struct menu_item *self)
{
    return post_bt_toggle_gui(self);
}

esp_err_t keyboard_gui_bt_toggle_action(void *user_ctx)
{
    ESP_LOGI(TAG, "Bluetooth toggle action triggered");

    if (!user_ctx) {
        ESP_LOGE(TAG, "Invalid Bluetooth toggle user context");
        return ESP_ERR_INVALID_ARG;
    }

    bt_toggle_gui_t *gui = (bt_toggle_gui_t *)user_ctx;

    // Toggle Bluetooth state
    if (is_ble_enabled()) {
        // Bluetooth is currently enabled, disable it
        ESP_LOGI(TAG, "Disabling Bluetooth");
        esp_err_t ret = update_ble_state_async(false, NULL);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to disable Bluetooth: %s", esp_err_to_name(ret));
            return ret;
        }

        // Update GUI to reflect disabled state
        lv_label_set_text(gui->status_label, "Open");
        lv_obj_set_style_image_recolor_opa(gui->bluetooth_icon, LV_OPA_50, 0);
        lv_obj_set_style_image_recolor(gui->bluetooth_icon, lv_color_hex(0x808080), 0);
    } else {
        // Bluetooth is currently disabled, enable it
        ESP_LOGI(TAG, "Enabling Bluetooth");
        const char *ble_name = get_ble_name();
        esp_err_t ret = update_ble_state_async(true, ble_name);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to enable Bluetooth: %s", esp_err_to_name(ret));
            return ret;
        }

        // Update GUI to reflect enabled state
        lv_label_set_text(gui->status_label, "Close");
        lv_obj_set_style_image_recolor_opa(gui->bluetooth_icon, LV_OPA_TRANSP, 0);
    }

    ESP_LOGI(TAG, "Bluetooth toggle action completed successfully");
    return ESP_OK;
}

esp_err_t keyboard_gui_prepare_bt_pair_kb(struct menu_item *self)
{
    return prepare_bt_pair_kb_gui(self);
}

esp_err_t keyboard_gui_post_bt_pair_kb(struct menu_item *self)
{
    return post_bt_pair_kb_gui(self);
}

bool keyboard_gui_bt_pair_kb_handle_input(void *user_ctx, input_event_e input_event, char key_code)
{
    return bt_pair_kb_handle_input_key(user_ctx, input_event, key_code);
}

void keyboard_gui_bt_pair_kb_prompt_for_passkey(struct menu_item *self, enum passkey_event_e event,
                                                esp_bd_addr_t bd_addr)
{
    bt_pair_kb_gui_t *gui = (bt_pair_kb_gui_t *)(self->user_ctx);

    switch (event)
    {
    case PASSKEY_CHALLENGE:
        if (self != menu_state_get_current_menu())
        {
            menu_navigate_to(self);
        }

        // user_ctx may be lazy allocated, so we need to get it again
        gui = (bt_pair_kb_gui_t *)(self->user_ctx);

        // Reset input buffer
        memset(gui->input_buffer, 0, sizeof(gui->input_buffer));
        gui->input_length = 0;
        // Enable passkey prompting
        gui->prompt_for_passkey = true;

        memcpy(gui->pair_bda, bd_addr, sizeof(esp_bd_addr_t));
        // Update passkey display to show initial prompt state
        update_passkey_display(gui);
        break;
    case PASSKEY_FAILURE:
        // if gui is null, just break
        if (!gui) {
            break;
        }
        // Reset input buffer and disable passkey prompting
        memset(gui->input_buffer, 0, sizeof(gui->input_buffer));
        gui->input_length = 0;
        gui->prompt_for_passkey = false;

        // Clear passkey display
        lv_label_set_text(gui->passkey_label, "passkey failed or cancelled");
        ESP_LOGI(TAG, "Passkey failed or cancelled");
        menu_state_set_timeout_ms(gui->last_timeout_ms);
        break;
    case PASSKEY_SUCCESS:
        // if gui is null, just break
        if (!gui) {
            break;
        }
        // Reset input buffer and disable passkey prompting
        memset(gui->input_buffer, 0, sizeof(gui->input_buffer));
        gui->input_length = 0;
        gui->prompt_for_passkey = false;
        // Clear passkey display
        lv_label_set_text(gui->passkey_label, "passkey matched");
        ESP_LOGI(TAG, "Passkey challenge succeeded");
        menu_state_set_timeout_ms(gui->last_timeout_ms);
        break;
    }
}

esp_err_t keyboard_gui_bt_pair_kb_action(void *user_ctx)
{
    ESP_LOGI(TAG, "BT pair keyboard action triggered");

    if (!user_ctx)
    {
        ESP_LOGE(TAG, "Invalid BT pair keyboard user context");
        return ESP_ERR_INVALID_ARG;
    }

    bt_pair_kb_gui_t *gui = (bt_pair_kb_gui_t *)user_ctx;

    // If we're prompting for passkey and have input, submit it
    if (gui->prompt_for_passkey && gui->input_length > 0)
    {
        // Convert input buffer to uint32_t
        uint32_t passkey = 0;
        for (int i = 0; i < gui->input_length; i++)
        {
            passkey = passkey * 10 + (gui->input_buffer[i] - '0');
        }

        ESP_LOGI(TAG, "Submitting passkey: 0x%lx", passkey);

        esp_err_t ret = esp_ble_passkey_reply(gui->pair_bda, true, passkey);
        // Reset input state
        memset(gui->input_buffer, 0, sizeof(gui->input_buffer));
        gui->input_length = 0;
        gui->prompt_for_passkey = false;

        if (ret == ESP_OK)
        {
            // Update display
            lv_label_set_text(gui->passkey_label, "Passkey confirmed");
        }
        else
        {
            lv_label_set_text(gui->passkey_label, "Passkey error");
        }

        return ret;
    }

    // If not in passkey mode, this could initiate pairing or perform other actions
    ESP_LOGI(TAG, "BT pair keyboard action completed");
    return ESP_OK;
}

// ============================================================================
// Report Target Selector GUI Implementation
// ============================================================================

static const char* target_conn_to_str(target_conn_e target)
{
    switch (target) {
        case TARGET_USB:
            return "USB";
        case TARGET_BLE:
            return "BLE";
        case TARGET_BROADCAST:
            return "Broadcast";
        default:
            return "Unknown";
    }
}

// Helper function to fetch and display BLE connection info
// If initialize_index is true, finds and sets the active connection index
// If initialize_index is false, uses the current ble_device_index (clamped to valid range)
static void fetch_and_display_conn_info(report_target_gui_t *gui, bool initialize_index)
{
    if (!gui) return;

    // Initialize broadcast mode state
    gui->is_broadcast_mode = false;

    if (gui->selected_target != 1) { // Not TARGET_BLE
        lv_label_set_text(gui->conn_info_label, "USB selected");
        return;
    }

    // TARGET_BLE - Get current BLE connections
    esp_hidd_dev_t* hid_dev = ble_get_hid_dev();
    if (!hid_dev) {
        gui->ble_device_count = 0;
        gui->ble_device_index = 0;
        lv_label_set_text(gui->conn_info_label, "BLE not ready");
        return;
    }

    // Check if in broadcast mode
    esp_err_t ret = esp_hidd_dev_is_broadcast_mode(hid_dev, &gui->is_broadcast_mode);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to query broadcast mode: %s", esp_err_to_name(ret));
        gui->is_broadcast_mode = false;
    }

    if (gui->is_broadcast_mode) {
        // In broadcast mode - no specific connection info
        gui->ble_device_count = 0;
        gui->ble_device_index = 0;
        gui->ble_conn_id = 0;
        memset(gui->ble_bda, 0, sizeof(esp_bd_addr_t));
        lv_label_set_text(gui->conn_info_label, "HID Broadcast Mode");
        if (initialize_index) {
            ESP_LOGI(TAG, "BLE in broadcast mode");
        }
        return;
    }

    // In unicast mode - get connections
    if (initialize_index) {
        gui->ble_device_count = 0;
        gui->ble_device_index = 0;
    }

    esp_hidd_conn_info_t conn_list[CONFIG_BT_ACL_CONNECTIONS];
    size_t count = 0;
    ret = esp_hidd_dev_get_connections(hid_dev, conn_list, CONFIG_BT_ACL_CONNECTIONS, &count);

    if (ret != ESP_OK) {
        gui->ble_device_count = 0;
        gui->ble_device_index = 0;
        lv_label_set_text(gui->conn_info_label, "BLE error");
        return;
    }

    if (count == 0) {
        gui->ble_device_count = 0;
        gui->ble_device_index = 0;
        lv_label_set_text(gui->conn_info_label, "No BLE connection");
        return;
    }

    // We have connections
    gui->ble_device_count = count;

    if (initialize_index) {
        // Find the active connection to set initial device index
        uint16_t active_conn_id = 0;
        ret = esp_hidd_dev_get_active_conn(hid_dev, &active_conn_id);
        if (ret == ESP_OK) {
            // Find the index of the active connection
            bool found = false;
            for (size_t i = 0; i < count; i++) {
                if (conn_list[i].conn_id == active_conn_id) {
                    gui->ble_device_index = i;
                    found = true;
                    break;
                }
            }
            if (found) {
                ESP_LOGI(TAG, "Active connection ID: %d at index %d", active_conn_id, gui->ble_device_index);
            }
        } else {
            ESP_LOGW(TAG, "Failed to get active connection: %s, using first connection", esp_err_to_name(ret));
        }
    } else {
        // Clamp device index to valid range
        if (gui->ble_device_index >= count) {
            gui->ble_device_index = count - 1;
        }
        if (gui->ble_device_index < 0) {
            gui->ble_device_index = 0;
        }
    }

    gui->ble_conn_id = conn_list[gui->ble_device_index].conn_id;
    memcpy(gui->ble_bda, conn_list[gui->ble_device_index].remote_bda, sizeof(esp_bd_addr_t));

    char info_text[80];
    snprintf(info_text, sizeof(info_text), "[%d/%d] ID:%d BDA:%02X:%02X:%02X:%02X:%02X:%02X",
             gui->ble_device_index + 1, (int)count, gui->ble_conn_id,
             gui->ble_bda[0], gui->ble_bda[1], gui->ble_bda[2],
             gui->ble_bda[3], gui->ble_bda[4], gui->ble_bda[5]);
    lv_label_set_text(gui->conn_info_label, info_text);
}

static void update_conn_info_display(report_target_gui_t *gui)
{
    fetch_and_display_conn_info(gui, false);
}

static void init_conn_info_display(report_target_gui_t *gui)
{
    fetch_and_display_conn_info(gui, true);
}

static void update_report_target_focus_style(report_target_gui_t *gui)
{
    if (!gui) return;

    // Set focused widget border
    lv_obj_set_style_border_width(gui->target_selector, 2, 0);
    lv_obj_set_style_border_color(gui->target_selector, lv_color_hex(0x0080FF), 0);
}

static report_target_gui_t* create_report_target_gui(void)
{
    report_target_gui_t *gui = malloc(sizeof(report_target_gui_t));
    if (!gui) {
        ESP_LOGE(TAG, "Failed to allocate report_target GUI");
        return NULL;
    }

    // Initialize GUI state
    target_conn_e current_target = get_report_target();
    gui->selected_target = (current_target == TARGET_USB) ? 0 : 1;
    gui->ble_device_index = 0;
    gui->ble_device_count = 0;
    gui->ble_conn_id = 0;
    gui->is_broadcast_mode = false;
    memset(gui->ble_bda, 0, sizeof(esp_bd_addr_t));

    // Create main container
    gui->container = lv_obj_create(lv_screen_active());
    lv_obj_set_size(gui->container, LCD_WIDTH, LCD_HEIGHT);
    lv_obj_set_pos(gui->container, 0, 0);
    lv_obj_set_style_bg_color(gui->container, lv_color_black(), 0);
    lv_obj_set_style_border_width(gui->container, 0, 0);
    lv_obj_set_style_pad_all(gui->container, 2, 0);
    lv_obj_clear_flag(gui->container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(gui->container, LV_OBJ_FLAG_HIDDEN);

    // Set horizontal layout - buttons on left, info on right
    lv_obj_set_flex_flow(gui->container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(gui->container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // Left section: Target label and selector buttons
    lv_obj_t *left_section = lv_obj_create(gui->container);
    lv_obj_set_size(left_section, 80, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(left_section, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(left_section, 0, 0);
    lv_obj_set_style_pad_all(left_section, 0, 0);
    lv_obj_set_flex_flow(left_section, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(left_section, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER);

    gui->target_selector = lv_buttonmatrix_create(left_section);
    static const char * target_map[] = {"USB", "BLE", ""};
    lv_buttonmatrix_set_map(gui->target_selector, target_map);
    lv_buttonmatrix_set_button_ctrl_all(gui->target_selector, LV_BUTTONMATRIX_CTRL_CHECKABLE);
    lv_buttonmatrix_set_one_checked(gui->target_selector, true);
    lv_buttonmatrix_set_button_ctrl(gui->target_selector, gui->selected_target, LV_BUTTONMATRIX_CTRL_CHECKED);
    lv_obj_set_size(gui->target_selector, 70, 28);

    // Style the button matrix - smaller font
    lv_obj_set_style_bg_color(gui->target_selector, lv_color_hex(0x333333), 0);
    lv_obj_set_style_text_color(gui->target_selector, lv_color_white(), 0);
    lv_obj_set_style_text_font(gui->target_selector, &lv_font_montserrat_10, 0);
    lv_obj_set_style_pad_all(gui->target_selector, 1, 0);
    lv_obj_set_style_pad_gap(gui->target_selector, 2, 0);

    // Style unselected buttons
    lv_obj_set_style_bg_color(gui->target_selector, lv_color_hex(0x555555), LV_PART_ITEMS);
    lv_obj_set_style_text_color(gui->target_selector, lv_color_white(), LV_PART_ITEMS);
    lv_obj_set_style_border_width(gui->target_selector, 1, LV_PART_ITEMS);
    lv_obj_set_style_border_color(gui->target_selector, lv_color_hex(0x777777), LV_PART_ITEMS);

    // Style selected buttons
    lv_obj_set_style_bg_color(gui->target_selector, lv_color_hex(0x0080FF), LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_text_color(gui->target_selector, lv_color_white(), LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_border_width(gui->target_selector, 2, LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_border_color(gui->target_selector, lv_color_hex(0x00AAFF), LV_PART_ITEMS | LV_STATE_CHECKED);

    // Right section: Connection info label
    gui->conn_info_label = lv_label_create(gui->container);
    lv_obj_set_style_text_color(gui->conn_info_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(gui->conn_info_label, &lv_font_montserrat_10, 0);
    lv_obj_set_style_margin_left(gui->conn_info_label, 5, 0);
    lv_obj_set_width(gui->conn_info_label, LCD_WIDTH - 90);
    lv_label_set_long_mode(gui->conn_info_label, LV_LABEL_LONG_DOT);
    lv_label_set_text(gui->conn_info_label, "");

    // Set initial focus styling
    update_report_target_focus_style(gui);

    return gui;
}

static esp_err_t prepare_report_target_gui(struct menu_item *self)
{
    ESP_LOGI(TAG, "Preparing report target GUI");

    if (!self) {
        return ESP_ERR_INVALID_ARG;
    }

    // Create GUI if not already created
    if (!self->user_ctx) {
        self->user_ctx = create_report_target_gui();
        if (!self->user_ctx) {
            ESP_LOGE(TAG, "Failed to create report target GUI");
            return ESP_ERR_NO_MEM;
        }
        ESP_LOGI(TAG, "Created new report target GUI");
    }

    report_target_gui_t *gui = (report_target_gui_t *)self->user_ctx;

    // Update to reflect current state
    target_conn_e current_target = get_report_target();
    gui->selected_target = (current_target == TARGET_USB) ? 0 : 1;
    lv_buttonmatrix_clear_button_ctrl(gui->target_selector, 0, LV_BUTTONMATRIX_CTRL_CHECKED);
    lv_buttonmatrix_clear_button_ctrl(gui->target_selector, 1, LV_BUTTONMATRIX_CTRL_CHECKED);
    lv_buttonmatrix_set_button_ctrl(gui->target_selector, gui->selected_target, LV_BUTTONMATRIX_CTRL_CHECKED);

    init_conn_info_display(gui);

    // Show the container
    lv_obj_remove_flag(gui->container, LV_OBJ_FLAG_HIDDEN);
    ESP_LOGI(TAG, "Showed report target container");

    return ESP_OK;
}

static esp_err_t post_report_target_gui(struct menu_item *self)
{
    ESP_LOGD(TAG, "Post report target GUI cleanup");

    if (!self || !self->user_ctx) {
        return ESP_OK;
    }

    report_target_gui_t *gui = (report_target_gui_t *)self->user_ctx;

    // Hide the container
    lv_obj_add_flag(gui->container, LV_OBJ_FLAG_HIDDEN);
    ESP_LOGI(TAG, "Hidden report target container");

    return ESP_OK;
}

static bool report_target_handle_input_key(void *user_ctx, input_event_e input_event, char key_code)
{
    report_target_gui_t *gui = (report_target_gui_t *)user_ctx;

    if (!gui) {
        ESP_LOGE(TAG, "GUI context is NULL");
        return false;
    }

    switch (input_event) {
        case INPUT_EVENT_LEFT_ARROW:
            if (gui->selected_target == 0) {
                // Already on USB, do nothing
                return true;
            } else if (gui->selected_target == 1) {
                // On BLE - check if in broadcast mode first
                if (gui->is_broadcast_mode) {
                    // In broadcast mode, only allow switching to USB
                    gui->selected_target = 0;
                    lv_buttonmatrix_clear_button_ctrl(gui->target_selector, 1, LV_BUTTONMATRIX_CTRL_CHECKED);
                    lv_buttonmatrix_set_button_ctrl(gui->target_selector, 0, LV_BUTTONMATRIX_CTRL_CHECKED);
                    update_conn_info_display(gui);
                } else {
                    // In unicast mode - check if we should switch to previous BLE device or USB
                    if (gui->ble_device_index > 0) {
                        // Switch to previous BLE device
                        gui->ble_device_index--;
                        update_conn_info_display(gui);
                    } else {
                        // At first BLE device, switch to USB
                        gui->selected_target = 0;
                        lv_buttonmatrix_clear_button_ctrl(gui->target_selector, 1, LV_BUTTONMATRIX_CTRL_CHECKED);
                        lv_buttonmatrix_set_button_ctrl(gui->target_selector, 0, LV_BUTTONMATRIX_CTRL_CHECKED);
                        update_conn_info_display(gui);
                    }
                }
            }
            return true;

        case INPUT_EVENT_RIGHT_ARROW:
            if (gui->selected_target == 0) {
                // On USB, move to first BLE device
                gui->selected_target = 1;
                gui->ble_device_index = 0;
                lv_buttonmatrix_clear_button_ctrl(gui->target_selector, 0, LV_BUTTONMATRIX_CTRL_CHECKED);
                lv_buttonmatrix_set_button_ctrl(gui->target_selector, 1, LV_BUTTONMATRIX_CTRL_CHECKED);
                update_conn_info_display(gui);
            } else if (gui->selected_target == 1) {
                // On BLE - only allow switching between devices if not in broadcast mode
                if (!gui->is_broadcast_mode && gui->ble_device_index < gui->ble_device_count - 1) {
                    // Switch to next BLE device
                    gui->ble_device_index++;
                    update_conn_info_display(gui);
                }
            }
            return true;

        default:
            break;
    }

    return false;
}

esp_err_t keyboard_gui_prepare_report_target(struct menu_item *self)
{
    return prepare_report_target_gui(self);
}

esp_err_t keyboard_gui_post_report_target(struct menu_item *self)
{
    return post_report_target_gui(self);
}

bool keyboard_gui_report_target_handle_input(void *user_ctx, input_event_e input_event, char key_code)
{
    return report_target_handle_input_key(user_ctx, input_event, key_code);
}

esp_err_t keyboard_gui_report_target_action(void *user_ctx)
{
    ESP_LOGI(TAG, "Report target action triggered");

    if (!user_ctx) {
        ESP_LOGE(TAG, "Invalid report target user context");
        return ESP_ERR_INVALID_ARG;
    }

    report_target_gui_t *gui = (report_target_gui_t *)user_ctx;

    target_conn_e target = (gui->selected_target == 0) ? TARGET_USB : TARGET_BLE;

    esp_err_t ret = ESP_OK;

    if (target == TARGET_BLE) {
        // Get IRK from current BLE connection
        const char* irk = get_ble_irk();

        // Set active connection (ble_conn_id can be 0, which is valid)
        esp_hidd_dev_t* hid_dev = ble_get_hid_dev();
        if (hid_dev) {
            ret = esp_hidd_dev_set_active_conn(hid_dev, gui->ble_conn_id);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "Failed to set active BLE connection: %s", esp_err_to_name(ret));
            }
        }

        ret = update_report_target(target, (const uint8_t*)irk, ESP_BT_OCTET16_LEN);
    } else {
        ret = update_report_target(target, NULL, 0);
    }

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to update report target: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "Report target updated to %s", target_conn_to_str(target));
    return ESP_OK;
}
