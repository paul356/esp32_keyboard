/*
 *  This program is a keyboard firmware for ESP family boards inspired by
 *  MK32 and qmk_firmware.
 *
 *  Copyright (C) 2024 panhao356@gmail.com
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

// Horizontal menu layout constants
#define MENU_ICON_WIDTH         72      // Width of each menu icon (reasonable size)
#define MENU_ICON_HEIGHT        72      // Height of each menu icon (reasonable size)
#define MENU_ICON_SPACING       8       // Spacing between icons
#define MENU_CONTAINER_PADDING  0       // No padding to maximize space
#define MAX_VISIBLE_ITEMS       ((LCD_WIDTH - 2 * MENU_CONTAINER_PADDING) / (MENU_ICON_WIDTH + MENU_ICON_SPACING))

#define TAG "gui_nonleaf_menu"

typedef struct {
    lv_obj_t *container;
    lv_obj_t *menu_items_container;
    lv_obj_t **menu_icons;          // Array of icon containers
    int visible_items_count;        // Number of currently visible items
    int first_visible_index;        // Index of first visible item
    int total_items_count;          // Total number of menu items
} nonleaf_item_gui_t;

static void update_nonleaf_item_display(nonleaf_item_gui_t *gui, struct menu_item *menu_item);
static nonleaf_item_gui_t* create_nonleaf_item_gui(void);
static lv_obj_t* create_menu_icon(lv_obj_t *parent, struct menu_item *menu_item, bool is_focused);
static void update_horizontal_menu_display(nonleaf_item_gui_t *gui, struct menu_item *menu_item);
static int count_menu_children(struct menu_item *menu_item);
static void calculate_visible_items(nonleaf_item_gui_t *gui, struct menu_item *menu_item);

static esp_err_t prepare_nonleaf_item_gui(struct menu_item *self);
static esp_err_t post_nonleaf_item_gui(struct menu_item *self);

static lv_obj_t* create_menu_icon(lv_obj_t *parent, struct menu_item *menu_item, bool is_focused)
{
    if (!menu_item) {
        return NULL;
    }

    const char *text = menu_item->text;

    // Create icon container
    lv_obj_t *icon_container = lv_obj_create(parent);
    lv_obj_set_size(icon_container, MENU_ICON_WIDTH, MENU_ICON_HEIGHT);
    lv_obj_set_style_radius(icon_container, 4, 0);  // Reduced radius to save space

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
    int container_width_pixels = LCD_WIDTH;  // Use full width since no padding
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

        // Position icon horizontally and center vertically
        int x_pos = icons_start_x + visible_item_index * (MENU_ICON_WIDTH + MENU_ICON_SPACING);
        int y_pos = (LCD_HEIGHT - MENU_ICON_HEIGHT) / 2;  // Center vertically in the display
        lv_obj_set_pos(icon, x_pos, y_pos);

        // Store icon reference
        gui->menu_icons[visible_item_index] = icon;
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
    gui->container = lv_obj_create(lv_screen_active());
    lv_obj_set_size(gui->container, LCD_WIDTH, LCD_HEIGHT);
    lv_obj_set_pos(gui->container, 0, 0);
    lv_obj_set_style_bg_color(gui->container, lv_color_black(), 0);
    lv_obj_set_style_border_width(gui->container, 0, 0);
    lv_obj_set_style_pad_all(gui->container, MENU_CONTAINER_PADDING, 0);  // No padding to maximize space

    // Disable scrollbars for root container
    lv_obj_clear_flag(gui->container, LV_OBJ_FLAG_SCROLLABLE);

    // Hide container initially - will be shown when prepare_gui_func is called
    lv_obj_add_flag(gui->container, LV_OBJ_FLAG_HIDDEN);

    // Horizontal menu items container
    gui->menu_items_container = lv_obj_create(gui->container);
    lv_coord_t container_width = LCD_WIDTH;  // Use full width since no padding
    lv_coord_t container_height = LCD_HEIGHT;  // Use full height to allow vertical centering
    lv_obj_set_size(gui->menu_items_container, container_width, container_height);

    // Position container to fill the entire screen
    lv_obj_set_pos(gui->menu_items_container, 0, 0);
    lv_obj_set_style_bg_color(gui->menu_items_container, lv_color_black(), 0);
    lv_obj_set_style_border_width(gui->menu_items_container, 0, 0);
    lv_obj_set_style_pad_all(gui->menu_items_container, 0, 0);

    // Disable scrollbars to prevent them from appearing
    lv_obj_clear_flag(gui->menu_items_container, LV_OBJ_FLAG_SCROLLABLE);

    // Allocate arrays for maximum possible visible items
    gui->menu_icons = malloc(MAX_VISIBLE_ITEMS * sizeof(lv_obj_t*));

    if (!gui->menu_icons) {
        ESP_LOGE(TAG, "Failed to allocate memory for menu arrays");
        free(gui);
        return NULL;
    }

    // Initialize arrays
    for (int i = 0; i < MAX_VISIBLE_ITEMS; i++) {
        gui->menu_icons[i] = NULL;
    }

    return gui;
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

    // Clean up horizontal menu icon arrays
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

static void update_nonleaf_item_display(nonleaf_item_gui_t *gui, struct menu_item *menu_item)
{
    if (!gui || !menu_item) {
        return;
    }

    // Update horizontal menu display
    update_horizontal_menu_display(gui, menu_item);
}

esp_err_t keyboard_gui_prepare_nonleaf_item(struct menu_item *self)
{
    return prepare_nonleaf_item_gui(self);
}

esp_err_t keyboard_gui_post_nonleaf_item(struct menu_item *self)
{
    return post_nonleaf_item_gui(self);
}