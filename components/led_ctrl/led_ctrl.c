/*
 * Copyright (C) 2026 panhao356@gmail.com
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "led_ctrl.h"
#include "drv_loop.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "keycode.h"
#include <string.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

/**
 * LED control event IDs
 */
typedef enum {
    LED_CTRL_EVENT_KEYSTROKE,  // Keystroke event received
    LED_CTRL_EVENT_CLEAR_LEDS,  // Clear all LEDs event
    LED_CTRL_EVENT_UPDATE_LEDS,  // Update/refresh LEDs event
    LED_CTRL_EVENT_ENABLE_RMT,  // Enable RMT hardware
    LED_CTRL_EVENT_DISABLE_RMT,  // Disable RMT hardware
    LED_CTRL_EVENT_MAX
} led_ctrl_event_id_t;

typedef struct {
    uint16_t keycode;
    uint8_t row;
    uint8_t col;
    bool pressed;  // True if key is pressed, false if released
} led_ctrl_keystroke_t;

/**
 * Pattern configuration structure
 */
typedef struct {
    led_pattern_type_e pattern;     // Pattern type
    led_drv_color_t primary_color;  // Primary color for pattern
    led_drv_color_t secondary_color; // Secondary color (for gradients, etc.)
    uint8_t brightness;             // Overall brightness (0-255)
    uint32_t param1;                // Pattern-specific parameter 1
    uint32_t param2;                // Pattern-specific parameter 2
} led_pattern_config_t;

// Component state
static bool s_initialized = false;
static bool s_rmt_enabled = false;  // Track RMT hardware state (initially enabled)

static const char *TAG = "led_ctrl";

// Event base definition
ESP_EVENT_DEFINE_BASE(LED_CTRL_EVENTS);

// Per-pattern default parameters (used when caller passes 0)
static const struct {
    uint32_t param1;
    uint32_t param2;
} s_pattern_defaults[LED_PATTERN_MAX] = {
    [LED_PATTERN_OFF]         = {0,    0},
    [LED_PATTERN_RAINBOW]     = {50,   0},   // speed
    [LED_PATTERN_RAINDROP]    = {5,    0},   // spawn rate (1/N)
    [LED_PATTERN_RIPPLE]      = {10,   30},  // speed, max_age (frames)
    [LED_PATTERN_FIRE]        = {180,  40},  // intensity, spark_chance
    [LED_PATTERN_BREATHING]   = {3000, 0},   // period_ms
    [LED_PATTERN_WAVE]        = {50,   0},   // speed
    [LED_PATTERN_HIT_KEY]     = {2000, 0},   // hold_time_ms
    [LED_PATTERN_SNAKE]       = {0,    0},   // no params
    [LED_PATTERN_TEXT_SCROLL] = {40,   0},   // scroll_speed
};

// Current pattern configuration
static led_pattern_config_t s_current_pattern = {
    .pattern = LED_PATTERN_OFF,
    .primary_color = LED_COLOR_BLACK,
    .secondary_color = LED_COLOR_BLACK,
    .brightness = 128,
    .param1 = 5,
    .param2 = 0
};

// Periodic timer for time-based patterns (breathing, wave, ripple decay)
static esp_timer_handle_t s_led_timer = NULL;
#define LED_TIMER_PERIOD_US  200000  // 5 fps

// Ripple effect state
#define MAX_RIPPLES 5
typedef struct {
    int8_t led_index;   // Center LED of the ripple, -1 if inactive
    uint8_t age;        // Frames since creation
    uint16_t hue;       // HSV hue for this ripple's color
} ripple_t;
static ripple_t s_ripples[MAX_RIPPLES] = {{-1, 0, 0}};

// ============================================================
//  Snake state
// ============================================================
#define SNAKE_MAX_LEN 30
static int8_t s_snake_body[SNAKE_MAX_LEN];  // LED indices of snake body, head at [0]
static uint8_t s_snake_len = 4;
static int8_t s_snake_dr = 0;              // grid row delta (-1/0/1)
static int8_t s_snake_dc = 1;              // grid col delta (-1/0/1)
static int8_t s_snake_food = -1;            // Food LED index

// ============================================================
//  Raindrop state
// ============================================================
#define MAX_DROPS 8
typedef struct {
    int8_t led_index;     // Current position, -1 if inactive
    uint8_t tail_len;     // Tail length
    led_drv_color_t color; // Drop color
} raindrop_t;
static raindrop_t s_drops[MAX_DROPS];

// ============================================================
//  Fire state — per-LED heat values
// ============================================================
static uint8_t s_fire_heat[LED_DRV_NUM_LEDS];

// ============================================================
//  Scroll text state
// ============================================================
#define SCROLL_TEXT_MAX 128
static char s_scroll_text[SCROLL_TEXT_MAX] = " MK32 ";
static int32_t s_scroll_offset = 0;   // pixel offset from right edge

// 5-row bitmap font — each char is 5 bytes (row 0..4), MSB=leftmost column
// Characters supported: 0-9, A-Z, space, ., !, <3 (heart)
typedef struct {
    char ch;
    uint8_t data[5];
    uint8_t width;  // columns (1..5)
} font_glyph_t;

static const font_glyph_t s_font[] = {
    // Digits
    {'0', {0b01110, 0b10001, 0b10001, 0b10001, 0b01110}, 5},
    {'1', {0b00100, 0b01100, 0b00100, 0b00100, 0b11111}, 5},
    {'2', {0b01110, 0b10001, 0b00110, 0b01000, 0b11111}, 5},
    {'3', {0b01110, 0b10001, 0b00110, 0b10001, 0b01110}, 5},
    {'4', {0b00010, 0b00110, 0b01010, 0b11111, 0b00010}, 5},
    {'5', {0b11111, 0b10000, 0b11110, 0b00001, 0b11110}, 5},
    {'6', {0b01110, 0b10000, 0b11110, 0b10001, 0b01110}, 5},
    {'7', {0b11111, 0b00001, 0b00010, 0b00100, 0b01000}, 5},
    {'8', {0b01110, 0b10001, 0b01110, 0b10001, 0b01110}, 5},
    {'9', {0b01110, 0b10001, 0b01111, 0b00001, 0b01110}, 5},
    // Uppercase letters
    {'A', {0b01110, 0b10001, 0b11111, 0b10001, 0b10001}, 5},
    {'B', {0b11110, 0b10001, 0b11110, 0b10001, 0b11110}, 5},
    {'C', {0b01110, 0b10000, 0b10000, 0b10000, 0b01110}, 5},
    {'D', {0b11110, 0b10001, 0b10001, 0b10001, 0b11110}, 5},
    {'E', {0b11111, 0b10000, 0b11100, 0b10000, 0b11111}, 5},
    {'F', {0b11111, 0b10000, 0b11100, 0b10000, 0b10000}, 5},
    {'G', {0b01110, 0b10000, 0b10111, 0b10001, 0b01110}, 5},
    {'H', {0b10001, 0b10001, 0b11111, 0b10001, 0b10001}, 5},
    {'I', {0b01110, 0b00100, 0b00100, 0b00100, 0b01110}, 5},
    {'J', {0b00111, 0b00001, 0b00001, 0b10001, 0b01110}, 5},
    {'K', {0b10001, 0b10010, 0b11100, 0b10010, 0b10001}, 5},
    {'L', {0b10000, 0b10000, 0b10000, 0b10000, 0b11111}, 5},
    {'M', {0b10001, 0b11011, 0b10101, 0b10001, 0b10001}, 5},
    {'N', {0b10001, 0b11001, 0b10101, 0b10011, 0b10001}, 5},
    {'O', {0b01110, 0b10001, 0b10001, 0b10001, 0b01110}, 5},
    {'P', {0b11110, 0b10001, 0b11110, 0b10000, 0b10000}, 5},
    {'Q', {0b01110, 0b10001, 0b10101, 0b10010, 0b01101}, 5},
    {'R', {0b11110, 0b10001, 0b11110, 0b10010, 0b10001}, 5},
    {'S', {0b01110, 0b10000, 0b01110, 0b00001, 0b11110}, 5},
    {'T', {0b11111, 0b00100, 0b00100, 0b00100, 0b00100}, 5},
    {'U', {0b10001, 0b10001, 0b10001, 0b10001, 0b01110}, 5},
    {'V', {0b10001, 0b10001, 0b10001, 0b01010, 0b00100}, 5},
    {'W', {0b10001, 0b10001, 0b10101, 0b11011, 0b10001}, 5},
    {'X', {0b10001, 0b01010, 0b00100, 0b01010, 0b10001}, 5},
    {'Y', {0b10001, 0b01010, 0b00100, 0b00100, 0b00100}, 5},
    {'Z', {0b11111, 0b00010, 0b00100, 0b01000, 0b11111}, 5},
    // Symbols
    {' ', {0b00000, 0b00000, 0b00000, 0b00000, 0b00000}, 3},
    {'.', {0b00000, 0b00000, 0b00000, 0b00000, 0b00100}, 2},
    {'!', {0b00100, 0b00100, 0b00100, 0b00000, 0b00100}, 2},
    // Heart
    { 3 ,  {0b01010, 0b11111, 0b11111, 0b01110, 0b00100}, 5},
};
#define FONT_GLYPH_COUNT (sizeof(s_font) / sizeof(s_font[0]))

// Forward declarations
static void led_ctrl_event_handler(void *event_handler_arg, esp_event_base_t event_base,
                                   int32_t event_id, void *event_data);
static void update_led_pattern(uint32_t frame);
static void refresh_led_pattern(void);
static led_drv_color_t apply_brightness(led_drv_color_t color, uint8_t brightness);
static void clear_all_leds(void);
static void draw_hit_key_pattern(uint32_t index);
static void draw_breathing_pattern(uint32_t frame);
static void draw_wave_pattern(uint32_t frame);
static void draw_ripple_pattern(uint32_t frame);
static void draw_rainbow_pattern(uint32_t frame);
static void draw_raindrop_pattern(uint32_t frame);
static void draw_snake_pattern(uint32_t frame);
static void draw_text_scroll_pattern(uint32_t frame);
static void draw_fire_pattern(uint32_t frame);
static void ripple_clear_all(void);
static void raindrop_clear_all(void);
static void snake_init(void);
static void snake_handle_key(uint16_t keycode);

/**
 * @brief Map matrix (row, col) to LED index using zigzag layout.
 *
 * Physical layout: 61 keys across 5 rows (14, 14, 13, 12, 8 keys).
 * LED numbering follows a serpentine (zigzag) pattern:
 *   - Even rows (0, 2, 4): right-to-left
 *   - Odd  rows (1, 3):    left-to-right
 *
 * @return LED index 0..60, or -1 if (row, col) has no LED
 */
static int8_t key_to_led(uint8_t row, uint8_t col) {
    static const uint8_t row_key_counts[] = {14, 14, 13, 12, 8};
    static const uint8_t row_led_start[]  = { 0, 14, 28, 41, 53};

    if (row >= 5) return -1;
    if (col >= row_key_counts[row]) return -1;

    uint8_t idx;
    if (row & 1) {  // odd row: left-to-right
        idx = row_led_start[row] + col;
    } else {        // even row: right-to-left
        idx = row_led_start[row] + (row_key_counts[row] - 1 - col);
    }

    return (idx < LED_DRV_NUM_LEDS) ? (int8_t)idx : -1;
}

// ============================================================
//  Grid helpers (Zigzag row/col ↔ LED index)
// ============================================================
static const uint8_t row_key_counts[] = {14, 14, 13, 12, 8};
static const uint8_t row_led_start[]  = { 0, 14, 28, 41, 53};

/** Inverse of key_to_led: LED index → (row, col). Returns bool. */
static bool led_to_row_col(int8_t led, uint8_t *row_out, uint8_t *col_out) {
    if (led < 0 || led >= LED_DRV_NUM_LEDS) return false;
    for (uint8_t r = 0; r < 5; r++) {
        if ((uint8_t)led < row_led_start[r]) continue;
        uint8_t count = row_key_counts[r];
        if ((uint8_t)led < row_led_start[r] + count) {
            uint8_t offset = (uint8_t)led - row_led_start[r];
            *row_out = r;
            *col_out = (r & 1) ? offset : (count - 1 - offset);
            return true;
        }
    }
    return false;
}

/** Set a single LED at (row, col). Safe no-op if invalid. */
static void set_led_xy(uint8_t row, uint8_t col, led_drv_color_t color) {
    int8_t idx = key_to_led(row, col);
    if (idx >= 0) led_drv_set_led((uint16_t)idx, color);
}

/** Navigate from LED index in grid direction. Returns -1 if off-grid. */
static int8_t led_move(int8_t led, int dr, int dc) {
    uint8_t r, c;
    if (!led_to_row_col(led, &r, &c)) return -1;
    int nr = (int)r + dr;
    int nc = (int)c + dc;
    if (nr < 0 || nr >= 5) return -1;
    if (nc < 0 || nc >= (int)row_key_counts[nr]) return -1;
    return key_to_led((uint8_t)nr, (uint8_t)nc);
}

// ============================================================
//  Physical key geometry (centi-units, ×100 of keycap "u" width)
//  Row total = 15.0u = 1500 centi-units for all 5 rows
// ============================================================

/** Key center x-position for each (row, col) in centi-units.
 *  Precomputed from key-width.txt — standard 60% layout. */
static const uint16_t key_center_x[5][14] = {
    {  50,  150,  250,  350,  450,  550,  650,  750,  850,  950, 1050, 1150, 1250, 1400},
    {  75,  200,  300,  400,  500,  600,  700,  800,  900, 1000, 1100, 1200, 1300, 1425},
    {  88,  225,  325,  425,  525,  625,  725,  825,  925, 1025, 1125, 1225, 1387},
    { 112,  275,  375,  475,  575,  675,  775,  875,  975, 1075, 1175, 1362},
    {  62,  188,  312,  688, 1062, 1188, 1312, 1438},
};

/** Find column in a given row whose center x is closest to target_x. */
static int8_t find_nearest_col(uint8_t row, uint16_t target_x) {
    uint8_t n = row_key_counts[row];
    int8_t best_c = 0;
    uint16_t best_diff = 1500;
    for (uint8_t c = 0; c < n; c++) {
        uint16_t cx = key_center_x[row][c];
        uint16_t diff = (cx > target_x) ? (cx - target_x) : (target_x - cx);
        if (diff < best_diff) {
            best_diff = diff;
            best_c = (int8_t)c;
        }
    }
    return best_c;
}

// ============================================================
//  Color helpers
// ============================================================

/** Convert HSV to RGB. h: 0-359, s: 0-255, v: 0-255 */
static led_drv_color_t hsv_to_rgb(uint16_t h, uint8_t s, uint8_t v) {
    if (s == 0) return LED_COLOR_RGB(v, v, v);
    uint8_t region = (uint8_t)(h / 60);
    uint8_t remainder = (uint8_t)((h % 60) * 255 / 60);
    uint8_t p = (uint8_t)((v * (255 - s)) >> 8);
    uint8_t q = (uint8_t)((v * (255 - ((uint16_t)s * remainder) / 255)) >> 8);
    uint8_t t = (uint8_t)((v * (255 - ((uint16_t)s * (255 - remainder)) / 255)) >> 8);
    switch (region) {
        case 0:  return LED_COLOR_RGB(v, t, p);
        case 1:  return LED_COLOR_RGB(q, v, p);
        case 2:  return LED_COLOR_RGB(p, v, t);
        case 3:  return LED_COLOR_RGB(p, q, v);
        case 4:  return LED_COLOR_RGB(t, p, v);
        default: return LED_COLOR_RGB(v, p, q);
    }
}

// ============================================================
//  RAINBOW — HSV sweep by physical x-position, cycling over time
// ============================================================
static void draw_rainbow_pattern(uint32_t frame) {
    uint32_t speed = s_current_pattern.param1;
    if (speed == 0) speed = 50;

    uint8_t sat = 255;
    uint8_t val = s_current_pattern.brightness;
    // 3 hue cycles across 1500 centi-units, shift each frame by speed
    uint16_t shift = (uint16_t)((frame * speed) % 360);

    led_drv_clear();
    for (int i = 0; i < LED_DRV_NUM_LEDS; i++) {
        uint8_t r, c;
        if (!led_to_row_col((int8_t)i, &r, &c)) continue;
        uint16_t cx = key_center_x[r][c];
        uint16_t hue = (shift + (uint16_t)((uint32_t)cx * 360 * 3 / 1500)) % 360;
        led_drv_set_led(i, hsv_to_rgb(hue, sat, val));
    }
    if (s_rmt_enabled) led_drv_update();
}

// ============================================================
//  RAINDROP — Matrix-style falling drops
// ============================================================
static void draw_raindrop_pattern(uint32_t frame) {
    uint32_t speed = s_current_pattern.param1;
    if (speed == 0) speed = 5;  // new drops per frame chance (1/N)

    // Spawn new drops at top row
    if ((frame % speed) == 0 && frame > 0) {
        for (int i = 0; i < MAX_DROPS; i++) {
            if (s_drops[i].led_index < 0) {
                uint8_t col = (uint8_t)((esp_random() % (uint32_t)row_key_counts[0]));
                s_drops[i].led_index = key_to_led(0, col);
                s_drops[i].tail_len = 3 + (uint8_t)(esp_random() % 5);
                // Random green-to-cyan color
                uint8_t g = 80 + (uint8_t)(esp_random() % 176);
                uint8_t b = 40 + (uint8_t)(esp_random() % 216);
                s_drops[i].color = LED_COLOR_RGB(0, g, b);
                break;
            }
        }
    }

    led_drv_clear();

    for (int d = 0; d < MAX_DROPS; d++) {
        if (s_drops[d].led_index < 0) continue;

        // Draw drop with tail going upward
        int8_t pos = s_drops[d].led_index;
        for (uint8_t t = 0; t < s_drops[d].tail_len; t++) {
            if (pos < 0) break;
            float brightness_f;
            if (t == 0) brightness_f = 1.0f;
            else brightness_f = 1.0f - (float)t / (float)s_drops[d].tail_len;
            uint8_t b = (uint8_t)(brightness_f * (float)s_current_pattern.brightness);
            led_drv_set_led((uint16_t)pos, apply_brightness(s_drops[d].color, b));
            // Move up one row in same column
            pos = led_move(pos, -1, 0);
        }

        // Advance drop downward
        int8_t next = led_move(s_drops[d].led_index, 1, 0);
        if (next < 0) {
            s_drops[d].led_index = -1;  // reached bottom
        } else {
            s_drops[d].led_index = next;
        }
    }

    if (s_rmt_enabled) led_drv_update();
}

static void raindrop_clear_all(void) {
    memset(s_drops, 0, sizeof(s_drops));
    for (int i = 0; i < MAX_DROPS; i++) s_drops[i].led_index = -1;
}

// ============================================================
//  SNAKE — Arrow-key controlled snake along LED strip
// ============================================================
static void snake_spawn_food(void) {
    int tries = 0;
    while (tries < 100) {
        int8_t candidate = (int8_t)(esp_random() % LED_DRV_NUM_LEDS);
        bool on_body = false;
        for (uint8_t i = 0; i < s_snake_len; i++) {
            if (s_snake_body[i] == candidate) { on_body = true; break; }
        }
        if (!on_body) { s_snake_food = candidate; return; }
        tries++;
    }
    s_snake_food = -1; // give up
}

static void snake_init(void) {
    s_snake_len = 3;
    // Start at LED 20 (middle of row 1), heading right in grid
    for (uint8_t i = 0; i < s_snake_len; i++) {
        s_snake_body[i] = 20 - (int8_t)i;
    }
    s_snake_dr = 0;
    s_snake_dc = 1;
    s_snake_food = -1;
    snake_spawn_food();
}

static void draw_snake_pattern(uint32_t frame) {
    if (s_snake_len == 0) { snake_init(); return; }

    // Move every frame (5 steps/sec at 5fps)
    int8_t head = s_snake_body[0];
    uint8_t hr, hc;
    if (!led_to_row_col(head, &hr, &hc)) { snake_init(); goto draw; }

    int nr = (int)hr + s_snake_dr;
    int nc = (int)hc + s_snake_dc;

    // Wrap row (5 rows total)
    if (nr < 0) nr = 4;
    else if (nr >= 5) nr = 0;

    // Column handling — use physical x-alignment for vertical moves
    {
        int ncols = (int)row_key_counts[nr];
        if (s_snake_dc != 0) {
            // Horizontal wrap
            if (nc < 0) nc = ncols - 1;
            else if (nc >= ncols) nc = 0;
        } else if (s_snake_dr != 0) {
            // Vertical: snap to nearest column by physical x-position
            uint16_t cx = key_center_x[hr][hc];
            nc = (int)find_nearest_col((uint8_t)nr, cx);
        } else {
            // Clamp
            if (nc < 0) nc = 0;
            else if (nc >= ncols) nc = ncols - 1;
        }
    }

    int8_t new_head = key_to_led((uint8_t)nr, (uint8_t)nc);

    // Check self-collision
    bool dead = false;
    for (uint8_t i = 1; i < s_snake_len; i++) {
        if (s_snake_body[i] == new_head) { dead = true; break; }
    }

    if (dead) {
        snake_init();
        goto draw;
    }

    // Shift body
    for (int i = (int)(s_snake_len - 1); i >= 0; i--) {
        s_snake_body[i + 1] = s_snake_body[i];
    }
    s_snake_body[0] = new_head;

    // Eat food
    if (new_head == s_snake_food) {
        if (s_snake_len < SNAKE_MAX_LEN) {
            s_snake_body[s_snake_len] = s_snake_body[s_snake_len - 1];
            s_snake_len++;
        }
        snake_spawn_food();
    }

draw:
    // Draw — head=RED, body=GREEN, tail=BLUE
    led_drv_clear();
    for (uint8_t i = 0; i < s_snake_len; i++) {
        led_drv_color_t seg_color;
        if (i == 0) {
            seg_color = LED_COLOR_RED;
        } else if (i == 1) {
            seg_color = LED_COLOR_GREEN;
        } else {
            seg_color = LED_COLOR_BLUE;
        }
        led_drv_set_led((uint16_t)s_snake_body[i],
            apply_brightness(seg_color, s_current_pattern.brightness));
    }
    if (s_snake_food >= 0) {
        // Blinking food: visible every other draw frame
        led_drv_set_led((uint16_t)s_snake_food,
            apply_brightness(LED_COLOR_RED, s_current_pattern.brightness));
    }
    if (s_rmt_enabled) led_drv_update();
}

static void snake_handle_key(uint16_t keycode) {
    if (s_snake_len == 0) return;

    int8_t dr = 0, dc = 0;

    switch (keycode) {
        case KC_UP:    dr = -1; dc =  0; break;
        case KC_DOWN:  dr =  1; dc =  0; break;
        case KC_LEFT:  dr =  0; dc = -1; break;
        case KC_RIGHT: dr =  0; dc =  1; break;
        default:       return;  // Not a direction key, ignore
    }

    // Reject 180° turn (would reverse into body)
    if (dr == -s_snake_dr && dc == -s_snake_dc) return;

    s_snake_dr = dr;
    s_snake_dc = dc;
}

// ============================================================
//  TEXT_SCROLL — Scrolling text on the 5-row "display"
// ============================================================
static const font_glyph_t* font_get_glyph(char ch) {
    for (size_t i = 0; i < FONT_GLYPH_COUNT; i++) {
        if (s_font[i].ch == ch) return &s_font[i];
    }
    return &s_font[0]; // fallback to '0'
}

static void draw_text_scroll_pattern(uint32_t frame) {
    uint32_t speed = s_current_pattern.param1;
    if (speed == 0) speed = 1;  // pixels per frame scroll speed
    uint8_t bri = s_current_pattern.brightness;

    // Move scroll offset (negative = moving leftward)
    s_scroll_offset -= (int32_t)speed;

    led_drv_clear();

    // Calculate total text width in pixels
    int text_px = 0;
    for (const char *p = s_scroll_text; *p; p++) {
        text_px += (int)font_get_glyph(*p)->width + 1; // +1 for spacing
    }
    if (text_px == 0) text_px = 1;

    // Wrap around
    while (s_scroll_offset < -text_px) s_scroll_offset += text_px;
    int off = s_scroll_offset;

    // Render characters
    int x = off;
    for (const char *p = s_scroll_text; *p; p++) {
        const font_glyph_t *g = font_get_glyph(*p);
        int char_end = x + (int)g->width;
        // Only render if char overlaps visible area (0..13, but we use -1..14 for partial)
        if (char_end > -1 && x < 15) {
            for (uint8_t cy = 0; cy < 5; cy++) {
                uint8_t row_data = g->data[cy];
                for (uint8_t cx = 0; cx < g->width; cx++) {
                    int col = x + (int)cx;
                    if (col < 0 || col >= 14) continue;
                    if (row_data & (1 << (4 - cx))) {
                        // Rainbow color based on column + frame (adjusted for 5fps)
                        uint16_t hue = (uint16_t)(((uint32_t)col * 5000 + frame * 1200) % 65536);
                        led_drv_color_t color = hsv_to_rgb(hue, 255, 255);
                        set_led_xy(cy, (uint8_t)col, apply_brightness(color, bri));
                    }
                }
            }
        }
        x += (int)g->width + 1;
    }

    if (s_rmt_enabled) led_drv_update();
}

// ============================================================
//  FIRE — Flame simulation with heat propagation
// ============================================================
static void draw_fire_pattern(uint32_t frame) {
    uint32_t intensity = s_current_pattern.param1;
    if (intensity == 0) intensity = 80; // 0-255 flame intensity
    uint8_t spark_chance = (uint8_t)(s_current_pattern.param2 ? s_current_pattern.param2 : 40);

    // Step 1: Seed bottom row (row 4) with random heat
    for (uint8_t c = 0; c < row_key_counts[4]; c++) {
        int8_t idx = key_to_led(4, c);
        if (idx < 0) continue;
        uint8_t r = (uint8_t)(esp_random() % 256);
        if (r < spark_chance) {
            s_fire_heat[idx] = (uint8_t)(220 + esp_random() % 36); // 220-255
        } else {
            s_fire_heat[idx] = (uint8_t)(s_fire_heat[idx] * 3 / 4);
        }
    }

    // Step 2: Propagate heat upward (from row 3 down to row 0)
    for (int r = 3; r >= 0; r--) {
        for (uint8_t c = 0; c < row_key_counts[r]; c++) {
            int8_t idx = key_to_led((uint8_t)r, c);
            if (idx < 0) continue;
            // Average heat from below (left, center, right) + spark
            uint32_t sum = 0;
            uint8_t cnt = 0;
            for (int dc = -1; dc <= 1; dc++) {
                int8_t below = key_to_led((uint8_t)(r + 1), (uint8_t)((int)c + dc));
                if (below >= 0) {
                    sum += s_fire_heat[below];
                    cnt++;
                }
            }
            if (cnt > 0) {
                uint16_t heat = (uint16_t)(sum / cnt * (uint32_t)intensity / 255);
                // Random cooling
                uint8_t cool = (uint8_t)(esp_random() % 40);
                s_fire_heat[idx] = (uint8_t)(heat > cool ? heat - cool : 0);
            }
        }
    }

    // Step 3: Map heat to LED colors
    led_drv_clear();
    for (int i = 0; i < LED_DRV_NUM_LEDS; i++) {
        uint8_t h = s_fire_heat[i];
        if (h == 0) continue;
        led_drv_color_t col;
        // Heat gradient: 0-85=black→red, 86-170=red→yellow, 171-255=yellow→white
        if (h < 85) {
            col = LED_COLOR_RGB(h * 3, 0, 0);
        } else if (h < 170) {
            col = LED_COLOR_RGB(255, (h - 85) * 3, 0);
        } else {
            col = LED_COLOR_RGB(255, 255, (h - 170) * 3);
        }
        uint8_t b = (uint8_t)((uint16_t)h * s_current_pattern.brightness / 255);
        led_drv_set_led(i, apply_brightness(col, b));
    }

    if (s_rmt_enabled) led_drv_update();
}

// ============================================================
//  Pattern drawing functions
// ============================================================

/**
 * @brief Breathing pattern — lights every 4th LED with sine brightness.
 *
 * Only ~15 LEDs are lit at once to keep power draw safe.
 */
static void draw_breathing_pattern(uint32_t frame) {
    uint32_t period_ms = s_current_pattern.param1;
    if (period_ms == 0) period_ms = 3000;

    // 5 fps → 200 ms/frame
    uint32_t period_frames = period_ms * 5 / 1000;
    if (period_frames == 0) period_frames = 1;

    float phase = (float)(frame % period_frames) / (float)period_frames * 2.0f * (float)M_PI;
    float brightness_f = (sinf(phase) + 1.0f) / 2.0f;   // 0.0 … 1.0

    // Slowly cycle hue: full rotation over ~20 seconds (100 frames at 5fps)
    uint16_t hue = (uint16_t)((frame % 100) * 65536 / 100);
    led_drv_color_t color = hsv_to_rgb(hue, 255, 255);
    led_drv_color_t dimmed = apply_brightness(color, (uint8_t)(brightness_f * (float)s_current_pattern.brightness));

    led_drv_clear();
    for (int i = (int)(frame % 4); i < LED_DRV_NUM_LEDS; i += 4) {
        led_drv_set_led(i, dimmed);
    }

    if (s_rmt_enabled) led_drv_update();
}

/**
 * @brief Wave pattern — a wave of ~10 LEDs sweeps across the strip.
 */
static void draw_wave_pattern(uint32_t frame) {
    uint32_t speed = s_current_pattern.param1;
    if (speed == 0) speed = 50;

    int wave_width = 10;

    // Move the wave center each frame (5fps)
    uint32_t pos = (frame * speed / 5) % LED_DRV_NUM_LEDS;

    led_drv_clear();
    for (int i = 0; i < wave_width; i++) {
        int idx = ((int)pos + i - wave_width / 2 + LED_DRV_NUM_LEDS) % LED_DRV_NUM_LEDS;
        float dist = fabsf((float)(i - wave_width / 2)) / (float)(wave_width / 2);
        float brightness_f = (1.0f - dist);
        if (brightness_f < 0.0f) brightness_f = 0.0f;
        // Rainbow gradient along the wave
        uint16_t hue = (uint16_t)((uint32_t)idx * 65536 / LED_DRV_NUM_LEDS);
        led_drv_color_t color = hsv_to_rgb(hue, 255, 255);
        led_drv_color_t dimmed = apply_brightness(color,
            (uint8_t)(brightness_f * (float)s_current_pattern.brightness));
        led_drv_set_led(idx, dimmed);
    }

    if (s_rmt_enabled) led_drv_update();
}

/**
 * @brief Ripple pattern — expanding rings from keypress positions.
 *
 * Each keystroke spawns a ripple that expands and fades over time.
 */
static void draw_ripple_pattern(uint32_t frame) {
    (void)frame;  // ripples use their own age counters

    uint32_t speed = s_current_pattern.param1;
    if (speed == 0) speed = 1;           // LEDs per frame expansion
    uint32_t max_age = s_current_pattern.param2;
    if (max_age == 0) max_age = 30;      // ripple lifetime in frames

    led_drv_clear();

    for (int r = 0; r < MAX_RIPPLES; r++) {
        if (s_ripples[r].led_index < 0) continue;

        s_ripples[r].age++;
        if (s_ripples[r].age > max_age) {
            s_ripples[r].led_index = -1;
            continue;
        }

        int center = s_ripples[r].led_index;
        int radius = (int)(s_ripples[r].age * speed);
        int thickness = 2;
        float fade = 1.0f - (float)s_ripples[r].age / (float)max_age;
        led_drv_color_t ripple_color = hsv_to_rgb(s_ripples[r].hue, 255, 255);

        for (int i = 0; i < LED_DRV_NUM_LEDS; i++) {
            int dist = abs(i - center);
            if (dist > LED_DRV_NUM_LEDS / 2)
                dist = LED_DRV_NUM_LEDS - dist;   // wrap-around distance

            if (dist >= radius && dist < radius + thickness) {
                uint8_t b = (uint8_t)(fade * (float)s_current_pattern.brightness);
                led_drv_color_t dimmed = apply_brightness(ripple_color, b);
                led_drv_set_led(i, dimmed);
            }
        }
    }

    if (s_rmt_enabled) led_drv_update();
}

/**
 * @brief Add a new ripple at the given LED index.
 */
static void ripple_add(int8_t led_index) {
    if (led_index < 0) return;
    // Replace oldest ripple
    int oldest = 0;
    for (int i = 1; i < MAX_RIPPLES; i++) {
        if (s_ripples[i].led_index < 0) {
            oldest = i;
            break;
        }
        if (s_ripples[i].age > s_ripples[oldest].age)
            oldest = i;
    }
    s_ripples[oldest].led_index = led_index;
    s_ripples[oldest].age = 0;
    s_ripples[oldest].hue = (uint16_t)(esp_random() % 65536);
}

/**
 * @brief Clear all ripples (called on pattern switch).
 */
static void ripple_clear_all(void) {
    for (int i = 0; i < MAX_RIPPLES; i++) {
        s_ripples[i].led_index = -1;
    }
}

// ============================================================
//  Periodic timer
// ============================================================

static void led_timer_callback(void *arg) {
    (void)arg;
    drv_loop_post_event(LED_CTRL_EVENTS, LED_CTRL_EVENT_UPDATE_LEDS, NULL, 0, 0);
}

static esp_err_t led_timer_start(void) {
    if (s_led_timer) return ESP_OK;

    esp_timer_create_args_t timer_args = {
        .callback = led_timer_callback,
        .name = "led_timer",
        .dispatch_method = ESP_TIMER_TASK,
    };
    esp_err_t ret = esp_timer_create(&timer_args, &s_led_timer);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create LED timer: %s", esp_err_to_name(ret));
        return ret;
    }
    ret = esp_timer_start_periodic(s_led_timer, LED_TIMER_PERIOD_US);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start LED timer: %s", esp_err_to_name(ret));
    }
    return ret;
}

static void led_timer_stop(void) {
    if (!s_led_timer) return;
    esp_timer_stop(s_led_timer);
    esp_timer_delete(s_led_timer);
    s_led_timer = NULL;
}

esp_err_t led_ctrl_init(void) {
    if (s_initialized) {
        ESP_LOGW(TAG, "LED control already initialized");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Initializing LED control component");

    // Initialize LED driver first
    esp_err_t ret = led_drv_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize LED driver: %s", esp_err_to_name(ret));
        return ret;
    }
    s_rmt_enabled = true;

    // Register event handler with drv_loop
    ret = drv_loop_register_handler(LED_CTRL_EVENTS, ESP_EVENT_ANY_ID,
                                   led_ctrl_event_handler, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register event handler: %s", esp_err_to_name(ret));
        return ret;
    }

    // Start periodic timer for time-based patterns
    ret = led_timer_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start LED timer: %s", esp_err_to_name(ret));
        return ret;
    }

    // Initialize all LEDs to off
    led_drv_clear();

    s_initialized = true;
    ESP_LOGI(TAG, "LED control component initialized successfully");

    drv_loop_post_event(LED_CTRL_EVENTS, LED_CTRL_EVENT_CLEAR_LEDS, NULL, 0, 0);

    return ESP_OK;
}

esp_err_t led_ctrl_set_pattern(led_pattern_type_e pattern_type, uint32_t param1, uint32_t param2) {
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    // Clear per-pattern state when switching
    if (pattern_type != LED_PATTERN_RIPPLE)  ripple_clear_all();
    if (pattern_type != LED_PATTERN_RAINDROP) raindrop_clear_all();
    if (pattern_type != LED_PATTERN_SNAKE)   s_snake_len = 0;
    if (pattern_type != LED_PATTERN_FIRE)    memset(s_fire_heat, 0, sizeof(s_fire_heat));

    // Initialize on entry
    if (pattern_type == LED_PATTERN_SNAKE)   snake_init();
    if (pattern_type == LED_PATTERN_TEXT_SCROLL) s_scroll_offset = 0;

    // Clear LEDs when switching to OFF
    if (pattern_type == LED_PATTERN_OFF) {
        clear_all_leds();
        led_timer_stop();  // No animation needed, save power
    } else if (s_rmt_enabled) {
        led_timer_start();  // Ensure timer is running for animated patterns
    }

    s_current_pattern.pattern = pattern_type;

    // Use per-pattern defaults when caller passes 0
    if (pattern_type < LED_PATTERN_MAX) {
        s_current_pattern.param1 = (param1 != 0) ? param1 : s_pattern_defaults[pattern_type].param1;
        s_current_pattern.param2 = (param2 != 0) ? param2 : s_pattern_defaults[pattern_type].param2;
    } else {
        s_current_pattern.param1 = param1;
        s_current_pattern.param2 = param2;
    }

    ESP_LOGI(TAG, "LED pattern set to %d with param1=%lu, param2=%lu", pattern_type, param1, param2);
    return ESP_OK;
}

esp_err_t led_ctrl_get_pattern(led_pattern_type_e *pattern_type, uint32_t *param1, uint32_t *param2) {
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (pattern_type == NULL || param1 == NULL || param2 == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    // Copy current pattern configuration to provided pointers
    *pattern_type = s_current_pattern.pattern;
    *param1 = s_current_pattern.param1;
    *param2 = s_current_pattern.param2;

    return ESP_OK;
}

esp_err_t led_ctrl_keystroke(uint16_t keycode, uint8_t row, uint8_t col, bool pressed) {
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (!pressed)
    {
        // only handle key press events now
        return ESP_OK;
    }

    if (s_current_pattern.pattern == LED_PATTERN_OFF) {
        // If the current pattern is OFF, we can just ignore the keystroke
        return ESP_OK;
    }

    ESP_LOGD(TAG, "Keystroke: keycode=0x%04X, pos=(%d,%d), pressed=%d", keycode, row, col, pressed);

    // Fill event structure and post to drv_loop
    led_ctrl_keystroke_t keystroke = {
        .keycode = keycode,
        .row = row,
        .col = col,
        .pressed = pressed,
    };

    return drv_loop_post_event(LED_CTRL_EVENTS, LED_CTRL_EVENT_KEYSTROKE,
                              &keystroke, sizeof(keystroke), 0);
}

/**
 * @brief Apply brightness scaling to a color
 */
static led_drv_color_t apply_brightness(led_drv_color_t color, uint8_t brightness) {
    led_drv_color_t result;
    result.red = (color.red * brightness) >> 8;
    result.green = (color.green * brightness) >> 8;
    result.blue = (color.blue * brightness) >> 8;
    return result;
}

static void draw_hit_key_pattern(uint32_t index)
{
    uint32_t prev = index - 1;
    // Example: Increment a counter for demonstration purposes
    if (prev >= LED_DRV_NUM_LEDS) {
        prev = LED_DRV_NUM_LEDS - 1;
    }

    // Rotating hue per keypress: each press shifts to a new color
    uint16_t hue = (uint16_t)((index * 7919) % 65536);  // prime multiplier for good distribution
    led_drv_color_t color = hsv_to_rgb(hue, 255, 255);

    // Apply brightness to colors
    led_drv_color_t key_color = apply_brightness(color, s_current_pattern.brightness);

    // Update LEDs with brightness-adjusted colors
    led_drv_set_led(prev, LED_COLOR_BLACK);
    led_drv_set_led(index, key_color);

    // Only update LED strip if RMT hardware is enabled
    if (s_rmt_enabled) {
        led_drv_update(); // Update the LED strip with the new colors
    } else {
        ESP_LOGD(TAG, "Skipping LED update - RMT disabled");
    }
}

/**
 * @brief Update LED pattern based on keystroke
 * This function updates the LED pattern based on the current pattern configuration
 */
static uint32_t current_frame;
static void update_led_pattern(uint32_t frame) {
    ESP_LOGD(TAG, "Updating LED pattern: frame=%lu", frame);

    current_frame = frame;
    uint32_t index = frame % LED_DRV_NUM_LEDS;
    draw_hit_key_pattern(index);

    ESP_LOGD(TAG, "Keystroke count: %lu", frame);
}

/**
 * @brief Refresh the current LED pattern
 * Called from the periodic timer via LED_CTRL_EVENT_UPDATE_LEDS.
 */
static void refresh_led_pattern(void) {
    if (!s_rmt_enabled) return;

    switch (s_current_pattern.pattern) {
        case LED_PATTERN_BREATHING:
            draw_breathing_pattern(current_frame);
            break;
        case LED_PATTERN_WAVE:
            draw_wave_pattern(current_frame);
            break;
        case LED_PATTERN_RIPPLE:
            draw_ripple_pattern(current_frame);
            break;
        case LED_PATTERN_RAINBOW:
            draw_rainbow_pattern(current_frame);
            break;
        case LED_PATTERN_RAINDROP:
            draw_raindrop_pattern(current_frame);
            break;
        case LED_PATTERN_SNAKE:
            draw_snake_pattern(current_frame);
            break;
        case LED_PATTERN_TEXT_SCROLL:
            draw_text_scroll_pattern(current_frame);
            break;
        case LED_PATTERN_FIRE:
            draw_fire_pattern(current_frame);
            break;
        case LED_PATTERN_HIT_KEY:
            // HIT_KEY is event-driven; timer refresh does nothing
            break;
        case LED_PATTERN_OFF:
        default:
            // OFF: do nothing
            break;
    }
}

static void clear_all_leds(void)
{
    led_drv_clear();
    // Only update if RMT is enabled
    if (s_rmt_enabled)
    {
        led_drv_update();
    }
}

static uint32_t s_count = 0;
static void handle_keystroke_event(uint16_t keycode, uint8_t row, uint8_t col, bool pressed) {
    ESP_LOGD(TAG, "Keystroke event: keycode=0x%04X, row=%d, col=%d, pressed=%d", keycode, row, col, pressed);

    switch (s_current_pattern.pattern) {
        case LED_PATTERN_HIT_KEY:
            update_led_pattern(s_count);
            ++s_count;
            break;
        case LED_PATTERN_RIPPLE: {
            int8_t led_idx = key_to_led(row, col);
            ripple_add(led_idx);
            break;
        }
        case LED_PATTERN_SNAKE:
            if (pressed) snake_handle_key(keycode);
            break;
        default:
            break;
    }
}

esp_err_t led_ctrl_set_brightness(uint8_t brightness) {
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (brightness > 100) {
        ESP_LOGE(TAG, "Brightness %d out of range (0-100)", brightness);
        return ESP_ERR_INVALID_ARG;
    }

    // Convert from 0-100 range to 0-255 range for internal use
    s_current_pattern.brightness = (brightness * 255) / 100;

    ESP_LOGI(TAG, "LED brightness set to %u%%", brightness);

    // Trigger a pattern refresh
    drv_loop_post_event(LED_CTRL_EVENTS, LED_CTRL_EVENT_UPDATE_LEDS, NULL, 0, 0);

    return ESP_OK;
}

esp_err_t led_ctrl_get_brightness(uint8_t *brightness) {
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (brightness == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    // Convert from 0-255 range to 0-100 range for external use
    *brightness = (s_current_pattern.brightness * 100) / 255;

    return ESP_OK;
}

esp_err_t led_ctrl_clear(void) {
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "Clearing all LEDs");

    // Trigger a pattern refresh if needed
    drv_loop_post_event(LED_CTRL_EVENTS, LED_CTRL_EVENT_CLEAR_LEDS, NULL, 0, 0);

    return ESP_OK;
}

esp_err_t led_ctrl_enable_rmt(void) {
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "Requesting RMT enable via event");

    // Post event to enable RMT in drv_loop context
    return drv_loop_post_event(LED_CTRL_EVENTS, LED_CTRL_EVENT_ENABLE_RMT, NULL, 0, 0);
}

esp_err_t led_ctrl_disable_rmt(void) {
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "Requesting RMT disable via event");

    // Post event to disable RMT in drv_loop context
    return drv_loop_post_event(LED_CTRL_EVENTS, LED_CTRL_EVENT_DISABLE_RMT, NULL, 0, 0);
}

bool led_ctrl_rmt_enabled(void) {
    return s_rmt_enabled;
}

// Main event handler
static void led_ctrl_event_handler(void *event_handler_arg, esp_event_base_t event_base,
                                   int32_t event_id, void *event_data) {
    if (event_base != LED_CTRL_EVENTS) {
        return;
    }

    switch (event_id) {
        case LED_CTRL_EVENT_KEYSTROKE: {
            if (event_data) {
                led_ctrl_keystroke_t *keystroke = (led_ctrl_keystroke_t *)event_data;
                ESP_LOGD(TAG, "Processing keystroke: pos=(%d,%d), pressed=%d",
                        keystroke->row, keystroke->col, keystroke->pressed);

                // Handle keystroke-based patterns if needed
                handle_keystroke_event(keystroke->keycode, keystroke->row, keystroke->col, keystroke->pressed);
            }
            break;
        }

        case LED_CTRL_EVENT_CLEAR_LEDS: {
            clear_all_leds();
            ESP_LOGI(TAG, "Clearing all LEDs");
            break;
        }

        case LED_CTRL_EVENT_UPDATE_LEDS: {
            current_frame++;
            refresh_led_pattern();
            break;
        }

        case LED_CTRL_EVENT_ENABLE_RMT: {
            if (!s_rmt_enabled) {
                esp_err_t ret = led_drv_enable();
                if (ret == ESP_OK) {
                    s_rmt_enabled = true;
                    // Resume timer if pattern requires animation
                    if (s_current_pattern.pattern != LED_PATTERN_OFF) {
                        led_timer_start();
                    }
                    ESP_LOGI(TAG, "RMT hardware enabled");
                } else {
                    ESP_LOGE(TAG, "Failed to enable RMT hardware: %s", esp_err_to_name(ret));
                }
            } else {
                ESP_LOGD(TAG, "RMT hardware already enabled");
            }
            break;
        }

        case LED_CTRL_EVENT_DISABLE_RMT: {
            if (s_rmt_enabled) {
                led_timer_stop();  // No need to refresh when RMT is off
                esp_err_t ret = led_drv_disable();
                if (ret == ESP_OK) {
                    s_rmt_enabled = false;
                    ESP_LOGI(TAG, "RMT hardware disabled");
                } else {
                    ESP_LOGE(TAG, "Failed to disable RMT hardware: %s", esp_err_to_name(ret));
                }
            } else {
                ESP_LOGD(TAG, "RMT hardware already disabled");
            }
            break;
        }

        default:
            ESP_LOGW(TAG, "Unknown event ID: %ld", event_id);
            break;
    }
}
