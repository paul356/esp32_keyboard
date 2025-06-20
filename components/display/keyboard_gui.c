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

#include "keyboard_gui.h"
#include "lcd_hardware.h"
#include "menu_state_machine.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_check.h"
#include "lvgl.h"
#include <string.h>
#include <stdio.h>
#include "drv_loop.h"

#define UPDATE_LVGL_PERIOD_MS  250  // Update LVGL every second

static const char *TAG = "keyboard_gui";

/**
 * @brief Keyboard statistics structure
 */
typedef struct {
    uint32_t total_char_count;
    uint32_t session_char_count;
    uint32_t session_start_time;
    uint8_t last_char_typed;
} keyboard_stats_t;

// GUI context structures for different menu types
typedef struct {
    lv_obj_t *container;
    lv_obj_t *stats_label;
    lv_obj_t *total_count_label;
    lv_obj_t *char_count_label;
    lv_obj_t *last_char_label;
    lv_timer_t *update_timer;  // Timer for periodic updates
} keyboard_info_gui_t;

typedef struct {
    lv_obj_t *container;
    lv_obj_t *menu_title_label;
    lv_obj_t *menu_items_list;
    lv_obj_t *current_selection_bar;
} nonleaf_item_gui_t;

typedef struct {
    int8_t input_event;
    unsigned char keycode;  // Keycode of the pressed key
} input_event_data_t;

enum {
    UPDATE_LVGL_EVENT = 0,  // Update LVGL display
    GUI_INPUT_EVENT,
};

ESP_EVENT_DEFINE_BASE(KEYBOARD_GUI_EVENTS);

// Global state for GUI
static bool s_gui_initialized = false;
static lv_obj_t *s_main_screen = NULL;
static keyboard_stats_t s_keyboard_stats = {0};

// Forward declarations
static keyboard_info_gui_t* create_keyboard_info_gui(void);
static nonleaf_item_gui_t* create_nonleaf_item_gui(void);
static void update_keyboard_info_display(keyboard_info_gui_t *gui);
static void update_nonleaf_item_display(nonleaf_item_gui_t *gui, struct menu_item *menu_item);
static void keyboard_info_timer_cb(lv_timer_t *timer);

// GUI setup and teardown functions for menu items
static esp_err_t prepare_keyboard_info_gui(struct menu_item *self);
static esp_err_t post_keyboard_info_gui(struct menu_item *self);
static esp_err_t prepare_nonleaf_item_gui(struct menu_item *self);
static esp_err_t post_nonleaf_item_gui(struct menu_item *self);

static void update_lvgl_timer_func(void *arg)
{
    esp_err_t ret = drv_loop_post_event(
        KEYBOARD_GUI_EVENTS,
        UPDATE_LVGL_EVENT,
        NULL,
        0,
        UPDATE_LVGL_PERIOD_MS / 2 / portTICK_PERIOD_MS);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to post LVGL update event: %s", esp_err_to_name(ret));
    }
}

static void update_lvgl_event_handler(void *arg, esp_event_base_t base, int32_t id, void *event_data)
{
    if (base == KEYBOARD_GUI_EVENTS && id == UPDATE_LVGL_EVENT) {
        keyboard_gui_update();
    }
}

static void gui_input_event_handler(void *arg, esp_event_base_t base, int32_t id, void *event_data)
{
    if (base == KEYBOARD_GUI_EVENTS && id == GUI_INPUT_EVENT) {
        input_event_data_t* input_evt_data = ((input_event_data_t *)event_data);
        menu_state_process_event(input_evt_data->input_event, input_evt_data->keycode);
    }
}

static esp_err_t start_gui_routine_task(void)
{
    ESP_RETURN_ON_ERROR(drv_loop_register_handler(KEYBOARD_GUI_EVENTS, UPDATE_LVGL_EVENT, update_lvgl_event_handler, NULL), TAG, "Failed to register LVGL update event handler");

    ESP_RETURN_ON_ERROR(drv_loop_register_handler(KEYBOARD_GUI_EVENTS, GUI_INPUT_EVENT, gui_input_event_handler, NULL), TAG, "Failed to register LVGL update event handler");

    const esp_timer_create_args_t update_timer_args = {
        .callback = &update_lvgl_timer_func,
        .name = "update_lvgl_timer"
    };

    esp_timer_handle_t update_timer;
    ESP_RETURN_ON_ERROR(esp_timer_create(&update_timer_args, &update_timer), TAG, "Failed to create LVGL update timer");
    ESP_RETURN_ON_ERROR(esp_timer_start_periodic(update_timer, UPDATE_LVGL_PERIOD_MS * 1000), TAG, "Failed to start LVGL update timer");

    return ESP_OK;
}

esp_err_t keyboard_gui_init(void)
{
    if (s_gui_initialized) {
        ESP_LOGW(TAG, "Keyboard GUI already initialized");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Initializing keyboard GUI");

    // Initialize LCD hardware
    ESP_RETURN_ON_ERROR(lcd_hardware_init(), TAG, "LCD hardware init failed");

    // Initialize menu state machine
    if (!menu_state_init()) {
        ESP_LOGE(TAG, "Menu state machine init failed");
        return ESP_FAIL;
    }

    // Initialize statistics
    memset(&s_keyboard_stats, 0, sizeof(keyboard_stats_t));
    s_keyboard_stats.session_start_time = esp_timer_get_time() / 1000; // Convert to ms

    // Create main screen
    s_main_screen = lv_screen_active();
    lv_obj_set_style_bg_color(s_main_screen, lv_color_black(), 0);

    ESP_RETURN_ON_ERROR(start_gui_routine_task(), TAG, "Failed to start periodic LVGL update task");

    s_gui_initialized = true;

    // Initialize display with keyboard information
    ESP_LOGI(TAG, "Displaying initial keyboard information");
    menu_state_process_event(INPUT_EVENT_TIMEOUT, 0);

    ESP_LOGI(TAG, "Keyboard GUI initialized successfully");
    return ESP_OK;
}

void keyboard_gui_update(void)
{
    if (!s_gui_initialized) {
        return;
    }

    // Update menu timeout
    menu_state_update_timeout();

    // The menu state machine handles all state transitions via prepare_gui_func and post_gui_func
    // No special handling needed here - all menu items manage themselves

    // Handle LVGL tasks
    lv_timer_handler();
}

static void keyboard_gui_update_stats(uint8_t keycode)
{
    s_keyboard_stats.total_char_count++;
    s_keyboard_stats.session_char_count++;
    s_keyboard_stats.last_char_typed = keycode;
}

bool keyboard_gui_handle_key_input(uint8_t mods, uint8_t *scan_code, int code_len)
{
    if (!s_gui_initialized) {
        ESP_LOGW(TAG, "Keyboard GUI not initialized");
        return false;
    }

    if (menu_state_accept_keys())
    {
        for (int i = 0; i < code_len; i++)
        {
            if (scan_code[i] == 0)
            {
                continue; // Skip null codes
            }
            keyboard_gui_post_input_event_isr(INPUT_EVENT_KEYCODE, scan_code[i]);
        }
        return true;
    }
    else
    {
        for (int i = 0; i < code_len; i++)
        {
            if (scan_code[i] == 0)
            {
                continue; // Skip null codes
            }
            keyboard_gui_update_stats(scan_code[i]);
        }
    }

    return false;
}

esp_err_t keyboard_gui_post_input_event_isr(input_event_e event, unsigned char keycode)
{
    input_event_data_t input_evt_data = {
        .input_event = event,
        .keycode = keycode
    };

    return drv_loop_post_event_isr(KEYBOARD_GUI_EVENTS, GUI_INPUT_EVENT, &input_evt_data, sizeof(input_evt_data));
}

void keyboard_gui_reset_session_stats(void)
{
    s_keyboard_stats.session_char_count = 0;
    s_keyboard_stats.session_start_time = esp_timer_get_time() / 1000;
    ESP_LOGI(TAG, "Session statistics reset");
}

esp_err_t keyboard_gui_set_brightness(uint8_t brightness)
{
    return lcd_hardware_set_backlight(brightness);
}

esp_err_t keyboard_gui_display_on_off(bool on)
{
    return lcd_hardware_display_on_off(on);
}

esp_err_t keyboard_gui_deinit(void)
{
    if (!s_gui_initialized) {
        return ESP_OK;
    }

    // Clean up LVGL objects
    if (s_main_screen) {
        lv_obj_clean(s_main_screen);
    }

    // Deinitialize LCD hardware
    lcd_hardware_deinit();

    s_gui_initialized = false;
    ESP_LOGI(TAG, "Keyboard GUI deinitialized");

    return ESP_OK;
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

    // Last key label
    gui->last_char_label = lv_label_create(gui->container);
    lv_obj_set_style_text_color(gui->last_char_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(gui->last_char_label, &lv_font_montserrat_12, 0);  // Changed to available font
    lv_obj_align(gui->last_char_label, LV_ALIGN_TOP_LEFT, 0, 82);  // Adjusted spacing to accommodate total count
    lv_label_set_text(gui->last_char_label, "Last: -");

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

static nonleaf_item_gui_t* create_nonleaf_item_gui(void)
{
    nonleaf_item_gui_t *gui = malloc(sizeof(nonleaf_item_gui_t));
    if (!gui) {
        ESP_LOGE(TAG, "Failed to allocate nonleaf item GUI");
        return NULL;
    }

    // Create container for nonleaf item
    gui->container = lv_obj_create(s_main_screen);
    lv_obj_set_size(gui->container, LCD_WIDTH, LCD_HEIGHT);
    lv_obj_set_pos(gui->container, 0, 0);
    lv_obj_set_style_bg_color(gui->container, lv_color_black(), 0);
    lv_obj_set_style_border_width(gui->container, 0, 0);
    lv_obj_set_style_pad_all(gui->container, 2, 0);

    // Hide container initially - will be shown when prepare_gui_func is called
    lv_obj_add_flag(gui->container, LV_OBJ_FLAG_HIDDEN);

    // Menu title
    gui->menu_title_label = lv_label_create(gui->container);
    lv_obj_set_style_text_color(gui->menu_title_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(gui->menu_title_label, &lv_font_montserrat_12, 0);
    lv_obj_align(gui->menu_title_label, LV_ALIGN_TOP_MID, 0, 0);
    lv_label_set_text(gui->menu_title_label, "Menu");

    // Menu items container
    gui->menu_items_list = lv_obj_create(gui->container);
    lv_obj_set_size(gui->menu_items_list, LCD_WIDTH - 4, LCD_HEIGHT - 30);
    lv_obj_set_pos(gui->menu_items_list, 2, 15);
    lv_obj_set_style_bg_color(gui->menu_items_list, lv_color_black(), 0);
    lv_obj_set_style_border_width(gui->menu_items_list, 0, 0);
    lv_obj_set_style_pad_all(gui->menu_items_list, 2, 0);

    // Selection highlight bar
    gui->current_selection_bar = lv_obj_create(gui->menu_items_list);
    lv_obj_set_size(gui->current_selection_bar, LCD_WIDTH - 8, 12);
    lv_obj_set_style_bg_color(gui->current_selection_bar, lv_color_hex(0x333333), 0);
    lv_obj_set_style_border_width(gui->current_selection_bar, 1, 0);
    lv_obj_set_style_border_color(gui->current_selection_bar, lv_color_white(), 0);

    // Instructions
    lv_obj_t *instruction_label = lv_label_create(gui->container);
    lv_obj_set_style_text_color(instruction_label, lv_color_hex(0x808080), 0);
    lv_obj_set_style_text_font(instruction_label, &lv_font_montserrat_8, 0);
    lv_obj_align(instruction_label, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_label_set_text(instruction_label, "Enter=Select ESC=Back");

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

    // Update last key
    if (s_keyboard_stats.last_char_typed != 0) {
        snprintf(buffer, sizeof(buffer), "Last: %c",
                 s_keyboard_stats.last_char_typed);
    }
    lv_label_set_text(gui->last_char_label, buffer);
}

static void update_nonleaf_item_display(nonleaf_item_gui_t *gui, struct menu_item *menu_item)
{
    if (!gui || !menu_item) {
        return;
    }

    // Update menu title
    lv_label_set_text(gui->menu_title_label, menu_item->text);

    // Clear existing menu items
    lv_obj_clean(gui->menu_items_list);

    // Recreate selection bar
    gui->current_selection_bar = lv_obj_create(gui->menu_items_list);
    lv_obj_set_size(gui->current_selection_bar, LCD_WIDTH - 8, 12);
    lv_obj_set_style_bg_color(gui->current_selection_bar, lv_color_hex(0x333333), 0);
    lv_obj_set_style_border_width(gui->current_selection_bar, 1, 0);
    lv_obj_set_style_border_color(gui->current_selection_bar, lv_color_white(), 0);

    // Add menu items (children of current menu)
    struct menu_item *child;
    int y_pos = 0;
    int item_index = 0;

    LIST_FOREACH(child, &menu_item->children, entry) {
        if (y_pos >= LCD_HEIGHT - 35) {
            break; // Don't overflow the display
        }

        lv_obj_t *item_label = lv_label_create(gui->menu_items_list);
        lv_obj_set_style_text_color(item_label, lv_color_white(), 0);
        lv_obj_set_style_text_font(item_label, &lv_font_montserrat_10, 0);
        lv_obj_set_pos(item_label, 2, y_pos);
        lv_label_set_text(item_label, child->text);

        // Highlight current selection
        if (child == menu_item->focused_child) {
            lv_obj_set_pos(gui->current_selection_bar, 0, y_pos - 1);
        }

        y_pos += 12;
        item_index++;
    }

    // If no children, show that this is a leaf menu
    if (LIST_EMPTY(&menu_item->children)) {
        lv_obj_t *info_label = lv_label_create(gui->menu_items_list);
        lv_obj_set_style_text_color(info_label, lv_color_hex(0x808080), 0);
        lv_obj_set_style_text_font(info_label, &lv_font_montserrat_10, 0);
        lv_obj_set_pos(info_label, 2, 0);
        lv_label_set_text(info_label, "Press ENTER to execute");

        // Hide selection bar for leaf menus
        lv_obj_add_flag(gui->current_selection_bar, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_remove_flag(gui->current_selection_bar, LV_OBJ_FLAG_HIDDEN);
    }
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
    ESP_LOGI(TAG, "Hidden nonleaf item container for: %s", self->text);

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
