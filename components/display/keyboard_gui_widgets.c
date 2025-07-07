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
#include "esp_timer.h"
#include "lvgl.h"
#include "lcd_hardware.h"
#include "menu_state_machine.h"
#include "keyboard_gui_widgets.h"
#include "menu_icons.h"
#include "drv_loop.h"
#include "nvs_io.h"
#include "nvs.h"
#include "function_control.h"

#define TAG "keyboard_gui_construct"

// Horizontal menu layout constants
#define MENU_ICON_WIDTH         76      // Width of each menu icon
#define MENU_ICON_HEIGHT        76      // Height of each menu icon
#define MENU_ICON_SPACING       8       // Spacing between icons
#define MENU_TEXT_HEIGHT        12      // Height for text below icons
#define MENU_CONTAINER_PADDING  4       // Padding around menu container
#define MAX_VISIBLE_ITEMS       ((LCD_WIDTH - 2 * MENU_CONTAINER_PADDING) / (MENU_ICON_WIDTH + MENU_ICON_SPACING))

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

// GUI context structures for different menu types
typedef struct {
    lv_obj_t *container;
    lv_obj_t *total_count_icon;     // Icon for total char count
    lv_obj_t *total_count_label;
    lv_obj_t *session_count_icon;   // Icon for session char count
    lv_obj_t *char_count_label;
    lv_timer_t *update_timer;  // Timer for periodic updates
} keyboard_info_gui_t;

typedef struct {
    lv_obj_t *container;
    lv_obj_t *menu_title_label;
    lv_obj_t *menu_items_container;
    lv_obj_t **menu_icons;          // Array of icon containers
    lv_obj_t **menu_labels;         // Array of text labels
    int visible_items_count;        // Number of currently visible items
    int first_visible_index;        // Index of first visible item
    int total_items_count;          // Total number of menu items
} nonleaf_item_gui_t;

typedef struct {
    lv_obj_t *container;
    lv_obj_t *bluetooth_icon;       // Bluetooth icon
    lv_obj_t *status_label;         // Label showing "Open" or "Close"
} bt_toggle_gui_t;

static lv_obj_t *s_main_screen = NULL;
static keyboard_stats_t s_keyboard_stats = {0};
static esp_timer_handle_t s_save_total_count_timer = NULL;

static keyboard_info_gui_t* create_keyboard_info_gui(void);
static nonleaf_item_gui_t* create_nonleaf_item_gui(void);
static bt_toggle_gui_t* create_bt_toggle_gui(void);
static void update_keyboard_info_display(keyboard_info_gui_t *gui);
static void update_nonleaf_item_display(nonleaf_item_gui_t *gui, struct menu_item *menu_item);
static void keyboard_info_timer_cb(lv_timer_t *timer);
static lv_obj_t* create_menu_icon(lv_obj_t *parent, struct menu_item *menu_item, bool is_focused);
static void update_horizontal_menu_display(nonleaf_item_gui_t *gui, struct menu_item *menu_item);
static int count_menu_children(struct menu_item *menu_item);
static void calculate_visible_items(nonleaf_item_gui_t *gui, struct menu_item *menu_item);

// NVS save functions
static void save_total_count_timer_func(void *arg);
static void save_total_count_event_handler(void *arg, esp_event_base_t base, int32_t id, void *event_data);
static esp_err_t load_total_count_from_nvs(void);
static esp_err_t save_total_count_to_nvs(void);

// GUI setup and teardown functions for menu items
static esp_err_t prepare_keyboard_info_gui(struct menu_item *self);
static esp_err_t post_keyboard_info_gui(struct menu_item *self);
static esp_err_t prepare_nonleaf_item_gui(struct menu_item *self);
static esp_err_t post_nonleaf_item_gui(struct menu_item *self);
static esp_err_t prepare_bt_toggle_gui(struct menu_item *self);
static esp_err_t post_bt_toggle_gui(struct menu_item *self);

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

static lv_obj_t* create_menu_icon(lv_obj_t *parent, struct menu_item *menu_item, bool is_focused)
{
    if (!menu_item) {
        return NULL;
    }

    const char *text = menu_item->text;

    // Create icon container
    lv_obj_t *icon_container = lv_obj_create(parent);
    lv_obj_set_size(icon_container, MENU_ICON_WIDTH, MENU_ICON_HEIGHT);
    lv_obj_set_style_radius(icon_container, 8, 0);

    // Disable scrollbars on icon container
    lv_obj_clear_flag(icon_container, LV_OBJ_FLAG_SCROLLABLE);

    // Set background color based on focus state
    if (is_focused) {
        lv_obj_set_style_bg_color(icon_container, lv_color_hex(0x404040), 0);
        lv_obj_set_style_border_width(icon_container, 1, 0);
        lv_obj_set_style_border_color(icon_container, lv_color_white(), 0);
    } else {
        lv_obj_set_style_bg_color(icon_container, lv_color_hex(0x202020), 0);
        lv_obj_set_style_border_width(icon_container, 0, 0);
        lv_obj_set_style_border_color(icon_container, lv_color_hex(0x404040), 0);
    }

    // Try to get image icon from menu item
    const lv_image_dsc_t *icon_img = menu_item->icon;

    if (icon_img) {
        // Create image widget
        lv_obj_t *img = lv_image_create(icon_container);
        lv_image_set_src(img, icon_img);

        // Size the image to use most of the container space (leave no padding)
        lv_obj_set_size(img, MENU_ICON_WIDTH, MENU_ICON_HEIGHT);
        lv_obj_center(img);

        // Optional: Recolor for focus state
        if (is_focused) {
            lv_obj_set_style_image_recolor_opa(img, LV_OPA_30, 0);
            lv_obj_set_style_image_recolor(img, lv_color_white(), 0);
        }
    } else {
        // Fallback to text-based icon (existing code)
        lv_obj_t *icon_label = lv_label_create(icon_container);
        lv_obj_set_style_text_color(icon_label, lv_color_white(), 0);
        lv_obj_set_style_text_font(icon_label, &lv_font_montserrat_10, 0);
        lv_obj_center(icon_label);

        // Create abbreviated text for icon - use first 3 characters
        char abbreviated_text[4] = {0};
        if (text) {
            strncpy(abbreviated_text, text, 3);
            abbreviated_text[3] = '\0';
        }
        lv_label_set_text(icon_label, abbreviated_text);
    }

    return icon_container;
}

static int count_menu_children(struct menu_item *menu_item)
{
    if (!menu_item || LIST_EMPTY(&menu_item->children)) {
        return 0;
    }

    int count = 0;
    struct menu_item *child;
    LIST_FOREACH(child, &menu_item->children, entry) {
        count++;
    }
    return count;
}

// Helper function to get menu item by circular index
static struct menu_item* get_menu_child_by_circular_index(struct menu_item *menu_item, int index, int total_count)
{
    if (!menu_item || total_count == 0) {
        return NULL;
    }

    // Wrap index to be within valid range
    while (index < 0) index += total_count;
    index = index % total_count;

    struct menu_item *child;
    int current_index = 0;
    LIST_FOREACH(child, &menu_item->children, entry) {
        if (current_index == index) {
            return child;
        }
        current_index++;
    }
    return NULL;
}

static void calculate_visible_items(nonleaf_item_gui_t *gui, struct menu_item *menu_item)
{
    if (!gui || !menu_item) {
        return;
    }

    gui->total_items_count = count_menu_children(menu_item);
    gui->visible_items_count = (gui->total_items_count < MAX_VISIBLE_ITEMS) ?
                               gui->total_items_count : MAX_VISIBLE_ITEMS;

    // Find focused child index
    int focused_index = 0;
    if (menu_item->focused_child) {
        struct menu_item *child;
        int index = 0;
        LIST_FOREACH(child, &menu_item->children, entry) {
            if (child == menu_item->focused_child) {
                focused_index = index;
                break;
            }
            index++;
        }
    }

    // For circular menu: always center the focused item
    // Calculate starting index to place focused item in the middle
    gui->first_visible_index = focused_index - (MAX_VISIBLE_ITEMS / 2);
}

static void update_horizontal_menu_display(nonleaf_item_gui_t *gui, struct menu_item *menu_item)
{
    if (!gui || !menu_item) {
        return;
    }

    // Clear existing items
    lv_obj_clean(gui->menu_items_container);

    // Calculate visible items
    calculate_visible_items(gui, menu_item);

    if (gui->total_items_count == 0) {
        // No children - show message
        lv_obj_t *info_label = lv_label_create(gui->menu_items_container);
        lv_obj_set_style_text_color(info_label, lv_color_hex(0x808080), 0);
        lv_obj_set_style_text_font(info_label, &lv_font_montserrat_10, 0);
        lv_obj_center(info_label);
        lv_label_set_text(info_label, "Press ENTER to execute");
        return;
    }

    // Calculate total width needed for visible icons
    int total_icons_width = gui->visible_items_count * MENU_ICON_WIDTH +
                           (gui->visible_items_count - 1) * MENU_ICON_SPACING;
    int container_width_pixels = LCD_WIDTH - 2 * MENU_CONTAINER_PADDING;
    int icons_start_x = (container_width_pixels - total_icons_width) / 2;

    // Create horizontal menu items using circular indexing
    for (int visible_item_index = 0; visible_item_index < gui->visible_items_count; visible_item_index++) {
        // Calculate circular index
        int circular_index = gui->first_visible_index + visible_item_index;

        // Get menu item using circular indexing
        struct menu_item *child = get_menu_child_by_circular_index(menu_item, circular_index, gui->total_items_count);
        if (!child) continue;

        // Create icon for this menu item
        bool is_focused = (child == menu_item->focused_child);
        if (is_focused) {
            ESP_LOGI(TAG, "Focused child: %s", child->text);
        }
        lv_obj_t *icon = create_menu_icon(gui->menu_items_container, child, is_focused);

        // Position icon horizontally - centered within container
        int x_pos = icons_start_x + visible_item_index * (MENU_ICON_WIDTH + MENU_ICON_SPACING);
        lv_obj_set_pos(icon, x_pos, 0);

        // Create text label below icon
        lv_obj_t *text_label = lv_label_create(gui->menu_items_container);
        lv_obj_set_style_text_color(text_label, is_focused ? lv_color_white() : lv_color_hex(0x808080), 0);
        lv_obj_set_style_text_font(text_label, &lv_font_montserrat_8, 0);
        lv_obj_set_pos(text_label, x_pos, MENU_ICON_HEIGHT + 2);
        lv_obj_set_width(text_label, MENU_ICON_WIDTH);
        lv_obj_set_style_text_align(text_label, LV_TEXT_ALIGN_CENTER, 0);

        // Truncate long text
        char truncated_text[9] = {0};
        if (strlen(child->text) > 8) {
            strncpy(truncated_text, child->text, 7);
            strcat(truncated_text, ".");
        } else {
            strcpy(truncated_text, child->text);
        }
        lv_label_set_text(text_label, truncated_text);

        // Store references
        gui->menu_icons[visible_item_index] = icon;
        gui->menu_labels[visible_item_index] = text_label;
    }
}

static nonleaf_item_gui_t* create_nonleaf_item_gui(void)
{
    nonleaf_item_gui_t *gui = malloc(sizeof(nonleaf_item_gui_t));
    if (!gui) {
        ESP_LOGE(TAG, "Failed to allocate nonleaf item GUI");
        return NULL;
    }

    // Initialize all pointers
    memset(gui, 0, sizeof(nonleaf_item_gui_t));

    // Create container for nonleaf item
    gui->container = lv_obj_create(s_main_screen);
    lv_obj_set_size(gui->container, LCD_WIDTH, LCD_HEIGHT);
    lv_obj_set_pos(gui->container, 0, 0);
    lv_obj_set_style_bg_color(gui->container, lv_color_black(), 0);
    lv_obj_set_style_border_width(gui->container, 0, 0);
    lv_obj_set_style_pad_all(gui->container, MENU_CONTAINER_PADDING, 0);

    // Disable scrollbars for root container
    lv_obj_clear_flag(gui->container, LV_OBJ_FLAG_SCROLLABLE);

    // Hide container initially - will be shown when prepare_gui_func is called
    lv_obj_add_flag(gui->container, LV_OBJ_FLAG_HIDDEN);

    // Menu title
    gui->menu_title_label = lv_label_create(gui->container);
    lv_obj_set_style_text_color(gui->menu_title_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(gui->menu_title_label, &lv_font_montserrat_12, 0);
    lv_obj_align(gui->menu_title_label, LV_ALIGN_CENTER, 0, -40);  // Position above centered container
    lv_label_set_text(gui->menu_title_label, "Menu");

    // Horizontal menu items container
    gui->menu_items_container = lv_obj_create(gui->container);
    lv_coord_t container_width = LCD_WIDTH - 2 * MENU_CONTAINER_PADDING;
    lv_coord_t container_height = MENU_ICON_HEIGHT + MENU_TEXT_HEIGHT + 16;  // Increased padding for larger icons
    lv_obj_set_size(gui->menu_items_container, container_width, container_height);

    // Center the container both horizontally and vertically
    lv_coord_t x_pos = (LCD_WIDTH - container_width) / 2;
    lv_coord_t y_pos = (LCD_HEIGHT - container_height) / 2;
    lv_obj_set_pos(gui->menu_items_container, x_pos, y_pos);
    lv_obj_set_style_bg_color(gui->menu_items_container, lv_color_black(), 0);
    lv_obj_set_style_border_width(gui->menu_items_container, 0, 0);
    lv_obj_set_style_pad_all(gui->menu_items_container, 0, 0);

    // Disable scrollbars to prevent them from appearing
    lv_obj_clear_flag(gui->menu_items_container, LV_OBJ_FLAG_SCROLLABLE);

    // Allocate arrays for maximum possible visible items
    gui->menu_icons = malloc(MAX_VISIBLE_ITEMS * sizeof(lv_obj_t*));
    gui->menu_labels = malloc(MAX_VISIBLE_ITEMS * sizeof(lv_obj_t*));

    if (!gui->menu_icons || !gui->menu_labels) {
        ESP_LOGE(TAG, "Failed to allocate memory for menu arrays");
        if (gui->menu_icons) free(gui->menu_icons);
        if (gui->menu_labels) free(gui->menu_labels);
        free(gui);
        return NULL;
    }

    // Initialize arrays
    for (int i = 0; i < MAX_VISIBLE_ITEMS; i++) {
        gui->menu_icons[i] = NULL;
        gui->menu_labels[i] = NULL;
    }

    // Instructions
    lv_obj_t *instruction_label = lv_label_create(gui->container);
    lv_obj_set_style_text_color(instruction_label, lv_color_hex(0x808080), 0);
    lv_obj_set_style_text_font(instruction_label, &lv_font_montserrat_8, 0);
    lv_obj_align(instruction_label, LV_ALIGN_BOTTOM_MID, 0, 50);  // Position below centered container
    lv_label_set_text(instruction_label, "Rotate=Navigate Enter=Select ESC=Back");

    return gui;
}

static bt_toggle_gui_t* create_bt_toggle_gui(void)
{
    bt_toggle_gui_t *gui = malloc(sizeof(bt_toggle_gui_t));
    if (!gui) {
        ESP_LOGE(TAG, "Failed to allocate bt_toggle GUI");
        return NULL;
    }

    // Create container for bt_toggle interface
    gui->container = lv_obj_create(s_main_screen);
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
    lv_obj_set_style_margin_right(gui->status_label, 20, 0);  // Gap between label and icon

    // Bluetooth icon
    gui->bluetooth_icon = lv_image_create(gui->container);
    lv_image_set_src(gui->bluetooth_icon, &bluetooth_icon);

    // Set initial label text and icon appearance based on current Bluetooth state
    if (is_ble_enabled()) {
        lv_label_set_text(gui->status_label, "Close");
        // Icon is colored normally when BT is enabled
        lv_obj_set_style_image_recolor_opa(gui->bluetooth_icon, LV_OPA_TRANSP, 0);
    } else {
        lv_label_set_text(gui->status_label, "Open");
        // Grey out the icon when BT is disabled
        lv_obj_set_style_image_recolor_opa(gui->bluetooth_icon, LV_OPA_50, 0);
        lv_obj_set_style_image_recolor(gui->bluetooth_icon, lv_color_hex(0x808080), 0);
    }

    return gui;
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

static void update_nonleaf_item_display(nonleaf_item_gui_t *gui, struct menu_item *menu_item)
{
    if (!gui || !menu_item) {
        return;
    }

    // Update menu title
    lv_label_set_text(gui->menu_title_label, menu_item->text);

    // Update horizontal menu display
    update_horizontal_menu_display(gui, menu_item);
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

    // Clean up horizontal menu icon and label arrays
    if (gui->menu_icons) {
        for (int i = 0; i < MAX_VISIBLE_ITEMS; i++) {
            if (gui->menu_icons[i]) {
                lv_obj_delete(gui->menu_icons[i]);
                gui->menu_icons[i] = NULL;
            }
        }
        free(gui->menu_icons);
        gui->menu_icons = NULL;
    }

    if (gui->menu_labels) {
        for (int i = 0; i < MAX_VISIBLE_ITEMS; i++) {
            if (gui->menu_labels[i]) {
                lv_obj_delete(gui->menu_labels[i]);
                gui->menu_labels[i] = NULL;
            }
        }
        free(gui->menu_labels);
        gui->menu_labels = NULL;
    }

    // Clean up the main container and GUI structure
    if (gui->container) {
        lv_obj_delete(gui->container);
        gui->container = NULL;
    }

    // Free the GUI structure itself
    free(gui);
    self->user_ctx = NULL;

    ESP_LOGI(TAG, "Completed cleanup for nonleaf item container: %s", self->text);

    return ESP_OK;
}

static esp_err_t prepare_bt_toggle_gui(struct menu_item *self)
{
    ESP_LOGI(TAG, "Preparing Bluetooth toggle GUI");

    if (!self) {
        return ESP_ERR_INVALID_ARG;
    }

    // Create Bluetooth toggle GUI if not already created
    if (!self->user_ctx) {
        self->user_ctx = create_bt_toggle_gui();
        if (!self->user_ctx) {
            ESP_LOGE(TAG, "Failed to create Bluetooth toggle GUI");
            return ESP_ERR_NO_MEM;
        }
        ESP_LOGI(TAG, "Created new Bluetooth toggle GUI");
    }

    bt_toggle_gui_t *gui = (bt_toggle_gui_t *)self->user_ctx;

    // Update label text and icon appearance based on current Bluetooth state
    if (is_ble_enabled()) {
        lv_label_set_text(gui->status_label, "Close");
        // Icon is colored normally when BT is enabled
        lv_obj_set_style_image_recolor_opa(gui->bluetooth_icon, LV_OPA_TRANSP, 0);
    } else {
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

    if (!self || !self->user_ctx) {
        return ESP_OK;
    }

    bt_toggle_gui_t *gui = (bt_toggle_gui_t *)self->user_ctx;

    // Hide the Bluetooth toggle container
    lv_obj_add_flag(gui->container, LV_OBJ_FLAG_HIDDEN);
    ESP_LOGI(TAG, "Hidden Bluetooth toggle container");

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

esp_err_t keyboard_gui_prepare_reset_meter(struct menu_item *self)
{
    ESP_LOGI(TAG, "Preparing reset meter GUI");

    if (!self) {
        return ESP_ERR_INVALID_ARG;
    }

    // Create container for reset meter GUI if not already created
    if (!self->user_ctx) {
        lv_obj_t *container = lv_obj_create(s_main_screen);
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

esp_err_t keyboard_gui_reset_meter_action(struct menu_item *self)
{
    ESP_LOGI(TAG, "Executing reset meter action");

    if (!self) {
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

esp_err_t keyboard_gui_prepare_bt_toggle(struct menu_item *self)
{
    return prepare_bt_toggle_gui(self);
}

esp_err_t keyboard_gui_post_bt_toggle(struct menu_item *self)
{
    return post_bt_toggle_gui(self);
}
