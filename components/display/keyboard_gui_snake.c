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

#include <stdlib.h>
#include <string.h>
#include "esp_err.h"
#include "esp_log.h"
#include "lvgl.h"
#include "display_hardware_info.h"
#include "menu_state_machine.h"
#include "keyboard_gui_widgets.h"

#define TAG "gui_snake"

// Grid: 8 px per cell, game area 280x72 at offset (2,2) within 284x76
#define CELL_SIZE       8
#define GRID_COLS       35   // 280 / 8
#define GRID_ROWS       9    // 72  / 8
#define GAME_AREA_X     2
#define GAME_AREA_Y     2
#define GAME_AREA_W     (GRID_COLS * CELL_SIZE)  // 280
#define GAME_AREA_H     (GRID_ROWS * CELL_SIZE)  // 72
#define MAX_CELLS       (GRID_COLS * GRID_ROWS)  // 315

#define GAME_TICK_MS    200  // milliseconds between game steps
#define INITIAL_LENGTH  4    // starting snake length

// Colors
#define COLOR_BG            lv_color_hex(0x000000)
#define COLOR_SNAKE_HEAD    lv_color_hex(0x00FF44)
#define COLOR_SNAKE_BODY    lv_color_hex(0x008822)
#define COLOR_FOOD          lv_color_hex(0xFF2200)
#define COLOR_GAMEOVER      lv_color_hex(0xFF4400)
#define COLOR_TEXT          lv_color_hex(0xFFFFFF)
#define COLOR_GRID_BORDER   lv_color_hex(0x1A1A1A)

typedef struct {
    int16_t col;
    int16_t row;
} snake_cell_t;

typedef enum {
    SNAKE_STATE_INIT,       // Waiting for ENTER to start
    SNAKE_STATE_PLAYING,
    SNAKE_STATE_GAME_OVER,  // Waiting for ENTER to restart
} snake_state_e;

typedef struct {
    lv_obj_t     *container;
    lv_obj_t     *game_area;      // Single widget; draw callback renders all state
    lv_obj_t     *score_label;
    lv_obj_t     *message_label;
    lv_timer_t   *game_timer;

    snake_cell_t  body[MAX_CELLS]; // Ring buffer: body[head_idx] is head
    int           head_idx;
    int           length;
    int           dx, dy;          // Current direction
    int           next_dx, next_dy;// Buffered direction (prevents 180° flip)
    snake_cell_t  food;
    int           score;
    snake_state_e state;

    uint32_t      saved_timeout_ms;
} snake_gui_t;

// ─── Helpers ────────────────────────────────────────────────────────────────

static int body_idx(const snake_gui_t *g, int offset)
{
    // offset 0 = head, offset 1 = next segment, ...
    return (g->head_idx - offset + MAX_CELLS) % MAX_CELLS;
}

static snake_cell_t body_cell(const snake_gui_t *g, int offset)
{
    return g->body[body_idx(g, offset)];
}

static bool cell_occupied(const snake_gui_t *g, int col, int row)
{
    for (int i = 0; i < g->length; i++) {
        snake_cell_t c = body_cell(g, i);
        if (c.col == col && c.row == row) return true;
    }
    return false;
}

static void spawn_food(snake_gui_t *g)
{
    int col, row;
    do {
        col = rand() % GRID_COLS;
        row = rand() % GRID_ROWS;
    } while (cell_occupied(g, col, row));
    g->food.col = col;
    g->food.row = row;
}

// ─── Draw callback ──────────────────────────────────────────────────────────

static void snake_draw_cb(lv_event_t *e)
{
    snake_gui_t *g = lv_event_get_user_data(e);
    lv_layer_t  *layer = lv_event_get_layer(e);
    lv_obj_t    *obj = lv_event_get_target_obj(e);

    lv_area_t obj_coords;
    lv_obj_get_coords(obj, &obj_coords);

    lv_draw_rect_dsc_t dsc;
    lv_draw_rect_dsc_init(&dsc);
    dsc.radius = 0;
    dsc.border_width = 0;

    // Background
    dsc.bg_color = COLOR_BG;
    lv_draw_rect(layer, &dsc, &obj_coords);

    if (g->state == SNAKE_STATE_INIT) return;

    // Food
    dsc.bg_color = COLOR_FOOD;
    lv_area_t food_area = {
        .x1 = obj_coords.x1 + g->food.col * CELL_SIZE + 1,
        .y1 = obj_coords.y1 + g->food.row * CELL_SIZE + 1,
        .x2 = obj_coords.x1 + g->food.col * CELL_SIZE + CELL_SIZE - 2,
        .y2 = obj_coords.y1 + g->food.row * CELL_SIZE + CELL_SIZE - 2,
    };
    lv_draw_rect(layer, &dsc, &food_area);

    // Snake body (draw from tail to head so head appears on top)
    for (int i = g->length - 1; i >= 0; i--) {
        snake_cell_t c = body_cell(g, i);
        dsc.bg_color = (i == 0) ? COLOR_SNAKE_HEAD : COLOR_SNAKE_BODY;
        lv_area_t seg = {
            .x1 = obj_coords.x1 + c.col * CELL_SIZE + 1,
            .y1 = obj_coords.y1 + c.row * CELL_SIZE + 1,
            .x2 = obj_coords.x1 + c.col * CELL_SIZE + CELL_SIZE - 2,
            .y2 = obj_coords.y1 + c.row * CELL_SIZE + CELL_SIZE - 2,
        };
        lv_draw_rect(layer, &dsc, &seg);
    }
}

// ─── Game logic ─────────────────────────────────────────────────────────────

static void snake_reset(snake_gui_t *g)
{
    g->length   = INITIAL_LENGTH;
    g->head_idx = INITIAL_LENGTH - 1;
    g->dx       = 1;
    g->dy       = 0;
    g->next_dx  = 1;
    g->next_dy  = 0;
    g->score    = 0;

    // Place snake horizontally at center
    int start_col = GRID_COLS / 2 - INITIAL_LENGTH + 1;
    int mid_row   = GRID_ROWS / 2;
    for (int i = 0; i < INITIAL_LENGTH; i++) {
        g->body[i].col = start_col + i;
        g->body[i].row = mid_row;
    }

    spawn_food(g);
}

static void update_score_label(snake_gui_t *g)
{
    char buf[32];
    snprintf(buf, sizeof(buf), "Score: %d", g->score);
    lv_label_set_text(g->score_label, buf);
}

static void snake_game_tick_cb(lv_timer_t *timer)
{
    snake_gui_t *g = lv_timer_get_user_data(timer);

    if (g->state != SNAKE_STATE_PLAYING) return;

    // Apply buffered direction
    g->dx = g->next_dx;
    g->dy = g->next_dy;

    // Compute new head position
    snake_cell_t head = body_cell(g, 0);
    int new_col = head.col + g->dx;
    int new_row = head.row + g->dy;

    // Wall collision
    if (new_col < 0 || new_col >= GRID_COLS || new_row < 0 || new_row >= GRID_ROWS) {
        g->state = SNAKE_STATE_GAME_OVER;
        lv_label_set_text(g->message_label, "GAME OVER\nENTER to restart");
        lv_obj_remove_flag(g->message_label, LV_OBJ_FLAG_HIDDEN);
        lv_obj_invalidate(g->game_area);
        return;
    }

    bool ate_food = (new_col == g->food.col && new_row == g->food.row);

    // Self collision (check all segments except the tail if not growing,
    // because the tail will vacate its cell this tick)
    int check_len = ate_food ? g->length : g->length - 1;
    for (int i = 0; i < check_len; i++) {
        snake_cell_t c = body_cell(g, i);
        if (c.col == new_col && c.row == new_row) {
            g->state = SNAKE_STATE_GAME_OVER;
            lv_label_set_text(g->message_label, "GAME OVER\nENTER to restart");
            lv_obj_remove_flag(g->message_label, LV_OBJ_FLAG_HIDDEN);
            lv_obj_invalidate(g->game_area);
            return;
        }
    }

    if (ate_food) {
        if (g->length < MAX_CELLS) {
            // Grow: advance head_idx without overwriting tail
            g->head_idx = (g->head_idx + 1) % MAX_CELLS;
            g->body[g->head_idx].col = new_col;
            g->body[g->head_idx].row = new_row;
            g->length++;
        }
        g->score++;
        update_score_label(g);
        spawn_food(g);
    } else {
        // Move: advance head_idx (naturally overwrites old tail slot)
        g->head_idx = (g->head_idx + 1) % MAX_CELLS;
        g->body[g->head_idx].col = new_col;
        g->body[g->head_idx].row = new_row;
    }

    lv_obj_invalidate(g->game_area);
}

// ─── GUI construction ────────────────────────────────────────────────────────

static snake_gui_t *create_snake_gui(void)
{
    snake_gui_t *g = malloc(sizeof(snake_gui_t));
    if (!g) {
        ESP_LOGE(TAG, "Failed to allocate snake_gui_t");
        return NULL;
    }
    memset(g, 0, sizeof(*g));

    // Full-screen container
    g->container = lv_obj_create(lv_screen_active());
    lv_obj_set_size(g->container, LCD_WIDTH, LCD_HEIGHT);
    lv_obj_set_pos(g->container, 0, 0);
    lv_obj_set_style_bg_color(g->container, COLOR_BG, 0);
    lv_obj_set_style_border_width(g->container, 0, 0);
    lv_obj_set_style_pad_all(g->container, 0, 0);
    lv_obj_clear_flag(g->container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(g->container, LV_OBJ_FLAG_HIDDEN);

    // Game area widget (all rendering done in draw callback)
    g->game_area = lv_obj_create(g->container);
    lv_obj_set_size(g->game_area, GAME_AREA_W, GAME_AREA_H);
    lv_obj_set_pos(g->game_area, GAME_AREA_X, GAME_AREA_Y);
    lv_obj_set_style_bg_color(g->game_area, COLOR_BG, 0);
    lv_obj_set_style_border_width(g->game_area, 0, 0);
    lv_obj_set_style_pad_all(g->game_area, 0, 0);
    lv_obj_clear_flag(g->game_area, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(g->game_area, snake_draw_cb, LV_EVENT_DRAW_MAIN, g);

    // Score label (top-right, fits in the 4px strip to the right of game area)
    g->score_label = lv_label_create(g->container);
    lv_obj_set_style_text_color(g->score_label, COLOR_TEXT, 0);
    lv_obj_set_style_text_font(g->score_label, &lv_font_montserrat_10, 0);
    lv_obj_set_pos(g->score_label, 0, 0);
    lv_label_set_text(g->score_label, "Score: 0");

    // Message label (centered overlay: "PRESS ENTER" / "GAME OVER")
    g->message_label = lv_label_create(g->container);
    lv_obj_set_style_text_color(g->message_label, COLOR_TEXT, 0);
    lv_obj_set_style_text_font(g->message_label, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_align(g->message_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(g->message_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(g->message_label, GAME_AREA_W);
    lv_obj_align(g->message_label, LV_ALIGN_CENTER, 0, 0);
    lv_label_set_text(g->message_label, "PRESS ENTER\nto start");

    // Periodic game timer (starts paused; will be enabled in prepare)
    g->game_timer = lv_timer_create(snake_game_tick_cb, GAME_TICK_MS, g);
    lv_timer_pause(g->game_timer);

    g->state = SNAKE_STATE_INIT;

    ESP_LOGI(TAG, "Snake GUI created");
    return g;
}

// ─── Public API ──────────────────────────────────────────────────────────────

esp_err_t keyboard_gui_prepare_snake(struct menu_item *self)
{
    if (!self) return ESP_ERR_INVALID_ARG;

    if (!self->user_ctx) {
        self->user_ctx = create_snake_gui();
        if (!self->user_ctx) return ESP_ERR_NO_MEM;
    }

    snake_gui_t *g = (snake_gui_t *)self->user_ctx;

    g->saved_timeout_ms = menu_state_get_timeout_ms();
    menu_state_set_timeout_ms(0);  // Disable menu auto-timeout during game

    lv_obj_remove_flag(g->container, LV_OBJ_FLAG_HIDDEN);
    lv_timer_resume(g->game_timer);

    ESP_LOGI(TAG, "Snake prepared");
    return ESP_OK;
}

esp_err_t keyboard_gui_post_snake(struct menu_item *self)
{
    if (!self || !self->user_ctx) return ESP_OK;

    snake_gui_t *g = (snake_gui_t *)self->user_ctx;

    lv_timer_pause(g->game_timer);
    menu_state_set_timeout_ms(g->saved_timeout_ms);
    lv_obj_add_flag(g->container, LV_OBJ_FLAG_HIDDEN);

    ESP_LOGI(TAG, "Snake post cleanup done");
    return ESP_OK;
}

bool keyboard_gui_snake_handle_input(void *user_ctx, input_event_e input_event, char key_code)
{
    (void)key_code;
    snake_gui_t *g = (snake_gui_t *)user_ctx;
    if (!g || g->state != SNAKE_STATE_PLAYING) return false;

    // Buffer direction change; prevent 180° reversal
    switch (input_event) {
    case INPUT_EVENT_UP_ARROW:
        if (g->dy != 1)  { g->next_dx = 0;  g->next_dy = -1; } break;
    case INPUT_EVENT_DOWN_ARROW:
        if (g->dy != -1) { g->next_dx = 0;  g->next_dy = 1;  } break;
    case INPUT_EVENT_LEFT_ARROW:
        if (g->dx != 1)  { g->next_dx = -1; g->next_dy = 0;  } break;
    case INPUT_EVENT_RIGHT_ARROW:
        if (g->dx != -1) { g->next_dx = 1;  g->next_dy = 0;  } break;
    default:
        return false;
    }
    return true;
}

esp_err_t keyboard_gui_snake_action(void *user_ctx)
{
    snake_gui_t *g = (snake_gui_t *)user_ctx;
    if (!g) return ESP_ERR_INVALID_ARG;

    if (g->state == SNAKE_STATE_INIT || g->state == SNAKE_STATE_GAME_OVER) {
        snake_reset(g);
        update_score_label(g);
        lv_obj_add_flag(g->message_label, LV_OBJ_FLAG_HIDDEN);
        g->state = SNAKE_STATE_PLAYING;
        lv_obj_invalidate(g->game_area);
    }
    return ESP_OK;
}
