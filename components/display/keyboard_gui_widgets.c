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

#define TAG "keyboard_gui_construct"

// Horizontal menu layout constants
#define MENU_ICON_WIDTH         48      // Width of each menu icon
#define MENU_ICON_HEIGHT        48      // Height of each menu icon
#define MENU_ICON_SPACING       8       // Spacing between icons
#define MENU_TEXT_HEIGHT        12      // Height for text below icons
#define MENU_CONTAINER_PADDING  4       // Padding around menu container
#define MAX_VISIBLE_ITEMS       ((LCD_WIDTH - 2 * MENU_CONTAINER_PADDING) / (MENU_ICON_WIDTH + MENU_ICON_SPACING))

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
    lv_obj_t *menu_items_container;
    lv_obj_t **menu_icons;          // Array of icon containers
    lv_obj_t **menu_labels;         // Array of text labels
    int visible_items_count;        // Number of currently visible items
    int first_visible_index;        // Index of first visible item
    int total_items_count;          // Total number of menu items
} nonleaf_item_gui_t;

static lv_obj_t *s_main_screen = NULL;
static keyboard_stats_t s_keyboard_stats = {0};

static keyboard_info_gui_t* create_keyboard_info_gui(void);
static nonleaf_item_gui_t* create_nonleaf_item_gui(void);
static void update_keyboard_info_display(keyboard_info_gui_t *gui);
static void update_nonleaf_item_display(nonleaf_item_gui_t *gui, struct menu_item *menu_item);
static void keyboard_info_timer_cb(lv_timer_t *timer);
static lv_obj_t* create_menu_icon(lv_obj_t *parent, const char *text, bool is_focused);
static void update_horizontal_menu_display(nonleaf_item_gui_t *gui, struct menu_item *menu_item);
static int count_menu_children(struct menu_item *menu_item);
static void calculate_visible_items(nonleaf_item_gui_t *gui, struct menu_item *menu_item);

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

static lv_obj_t* create_menu_icon(lv_obj_t *parent, const char *text, bool is_focused)
{
    // Create icon container
    lv_obj_t *icon_container = lv_obj_create(parent);
    lv_obj_set_size(icon_container, MENU_ICON_WIDTH, MENU_ICON_HEIGHT);
    lv_obj_set_style_radius(icon_container, 8, 0);
    
    // Set background color based on focus state
    if (is_focused) {
        lv_obj_set_style_bg_color(icon_container, lv_color_hex(0x404040), 0);
        lv_obj_set_style_border_width(icon_container, 2, 0);
        lv_obj_set_style_border_color(icon_container, lv_color_white(), 0);
    } else {
        lv_obj_set_style_bg_color(icon_container, lv_color_hex(0x202020), 0);
        lv_obj_set_style_border_width(icon_container, 1, 0);
        lv_obj_set_style_border_color(icon_container, lv_color_hex(0x404040), 0);
    }

    // Create text label inside icon - use first 3 characters or abbreviation
    lv_obj_t *icon_label = lv_label_create(icon_container);
    lv_obj_set_style_text_color(icon_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(icon_label, &lv_font_montserrat_10, 0);
    lv_obj_center(icon_label);
    
    // Create abbreviated text for icon
    char abbreviated_text[4] = {0};
    if (text) {
        if (strcmp(text, "Keyboard Mode") == 0) {
            strcpy(abbreviated_text, "KB");
        } else if (strcmp(text, "Bluetooth") == 0) {
            strcpy(abbreviated_text, "BT");
        } else if (strcmp(text, "WiFi") == 0) {
            strcpy(abbreviated_text, "WF");
        } else if (strcmp(text, "LED") == 0) {
            strcpy(abbreviated_text, "LED");
        } else if (strcmp(text, "Advanced") == 0) {
            strcpy(abbreviated_text, "ADV");
        } else if (strcmp(text, "About") == 0) {
            strcpy(abbreviated_text, "?");
        } else {
            // Use first 3 characters for other items
            strncpy(abbreviated_text, text, 3);
            abbreviated_text[3] = '\0';
        }
    }
    lv_label_set_text(icon_label, abbreviated_text);

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
    
    // Calculate first visible index to center the focused item when possible
    if (gui->total_items_count <= MAX_VISIBLE_ITEMS) {
        gui->first_visible_index = 0;
    } else {
        gui->first_visible_index = focused_index - (MAX_VISIBLE_ITEMS / 2);
        if (gui->first_visible_index < 0) {
            gui->first_visible_index = 0;
        } else if (gui->first_visible_index + MAX_VISIBLE_ITEMS > gui->total_items_count) {
            gui->first_visible_index = gui->total_items_count - MAX_VISIBLE_ITEMS;
        }
    }
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
    
    // Create horizontal menu items
    struct menu_item *child;
    int current_index = 0;
    int visible_item_index = 0;
    
    LIST_FOREACH(child, &menu_item->children, entry) {
        // Skip items before first visible
        if (current_index < gui->first_visible_index) {
            current_index++;
            continue;
        }
        
        // Stop if we've shown enough visible items
        if (visible_item_index >= gui->visible_items_count) {
            break;
        }
        
        // Create icon for this menu item
        bool is_focused = (child == menu_item->focused_child);
        lv_obj_t *icon = create_menu_icon(gui->menu_items_container, child->text, is_focused);
        
        // Position icon horizontally
        int x_pos = visible_item_index * (MENU_ICON_WIDTH + MENU_ICON_SPACING);
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
        
        current_index++;
        visible_item_index++;
    }
    
    // Show scroll indicators if needed
    if (gui->total_items_count > MAX_VISIBLE_ITEMS) {
        // Left scroll indicator
        if (gui->first_visible_index > 0) {
            lv_obj_t *left_arrow = lv_label_create(gui->menu_items_container);
            lv_obj_set_style_text_color(left_arrow, lv_color_white(), 0);
            lv_obj_set_style_text_font(left_arrow, &lv_font_montserrat_12, 0);
            lv_obj_set_pos(left_arrow, -15, MENU_ICON_HEIGHT / 2 - 6);
            lv_label_set_text(left_arrow, "<");
        }
        
        // Right scroll indicator
        if (gui->first_visible_index + gui->visible_items_count < gui->total_items_count) {
            lv_obj_t *right_arrow = lv_label_create(gui->menu_items_container);
            lv_obj_set_style_text_color(right_arrow, lv_color_white(), 0);
            lv_obj_set_style_text_font(right_arrow, &lv_font_montserrat_12, 0);
            lv_obj_set_pos(right_arrow, gui->visible_items_count * (MENU_ICON_WIDTH + MENU_ICON_SPACING), 
                          MENU_ICON_HEIGHT / 2 - 6);
            lv_label_set_text(right_arrow, ">");
        }
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

    // Hide container initially - will be shown when prepare_gui_func is called
    lv_obj_add_flag(gui->container, LV_OBJ_FLAG_HIDDEN);

    // Menu title
    gui->menu_title_label = lv_label_create(gui->container);
    lv_obj_set_style_text_color(gui->menu_title_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(gui->menu_title_label, &lv_font_montserrat_12, 0);
    lv_obj_align(gui->menu_title_label, LV_ALIGN_TOP_MID, 0, 0);
    lv_label_set_text(gui->menu_title_label, "Menu");

    // Horizontal menu items container
    gui->menu_items_container = lv_obj_create(gui->container);
    lv_obj_set_size(gui->menu_items_container, LCD_WIDTH - 2 * MENU_CONTAINER_PADDING, 
                    MENU_ICON_HEIGHT + MENU_TEXT_HEIGHT + 8);
    lv_obj_set_pos(gui->menu_items_container, 0, 20);
    lv_obj_set_style_bg_color(gui->menu_items_container, lv_color_black(), 0);
    lv_obj_set_style_border_width(gui->menu_items_container, 0, 0);
    lv_obj_set_style_pad_all(gui->menu_items_container, 0, 0);

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
    lv_obj_align(instruction_label, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_label_set_text(instruction_label, "Rotate=Navigate Enter=Select ESC=Back");

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
