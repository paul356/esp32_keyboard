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

#ifndef KEYMAP_C
#define KEYMAP_C

#include "key_definitions.h"
#include "keyboard_config.h"
#include "keymap.h"
#include "macros.h"
#include "function_key.h"
#include "quantum.h"

// A bit different from QMK, default returns you to the first layer, LOWER and raise increase/lower layer by order.
#define DEFAULT 0x100
#define LOWER 0x101
#define RAISE 0x102

// Keymaps are designed to be relatively interchangeable with QMK
enum custom_keycodes {
	QWERTY, NUM,
    PLUGINS,
};

// array to hold names of layouts for oled
char default_layout_names[LAYERS][MAX_LAYOUT_NAME_LENGTH] = {
    "Layer 0", "Layer 1", "Layer 2",
};

//NOTE: For this keymap due to wiring constraints the the two last rows on the left are wired unconventionally
// Each keymap is represented by an array, with an array that points to all the keymaps  by order
uint16_t _LAYERS[LAYERS][MATRIX_ROWS][MATRIX_COLS] = {
    {
        {KC_ESC,        KC_1,       KC_2,       KC_3,       KC_4,       KC_5,       KC_6,       KC_7,       KC_8,       KC_9,       KC_0,       KC_BSPC},
        {KC_TAB,        KC_Q,       KC_W,       KC_E,       KC_R,       KC_T,       KC_Y,       KC_U,       KC_I,       KC_O,       KC_P,       KC_LBRC},
        {KC_CAPS,       KC_A,       KC_S,       KC_D,       KC_F,       KC_G,       KC_H,       KC_J,       KC_K,       KC_L,       KC_SCOLON,  KC_ENTER},
        {LSFT_T(KC_ESC),KC_Z,       KC_X,       KC_C,       KC_V,       KC_B,       KC_N,       KC_M,       KC_COMM,    KC_DOT,     KC_SLASH,   RSFT_T(KC_QUOTE)},
        {KC_LCTRL,      KC_LGUI,    KC_LALT,   KC_MINUS, LT(1, KC_PGUP),KC_SPC,     KC_SPC,LT(2, KC_PGDOWN),KC_LEFT,    KC_UP,      KC_DOWN,    KC_RIGHT}
    },
    {
        {KC_GRAVE,      KC_F1,      KC_F2,      KC_F3,      KC_F4,      KC_F5,      KC_F6,      KC_F7,      KC_F8,      KC_F9,      KC_EQUAL,   KC_DEL},
        {_______,       KC_F10,     KC_F11,     KC_F12,     _______,    _______,    _______,    _______,    _______,    _______,    _______,    KC_RBRC},
        {_______,       _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______},
        {_______,       _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    KC_BSLASH,  _______},
        {_______,       _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    KC_PGUP,    KC_PGDN,    _______}
    },
    {
        {FUNCTION_KEY_INTRO, FUNCTION_KEY_DEVICE_INFO, FUNCTION_KEY_HOTSPOT, _______,    _______,    _______,    _______,    _______,    _______,    _______,    KC_MINUS,   KC_EQUAL},
        {_______,       _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______},
        {_______,       _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______},
        {_______,       _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______},
        {_______,       _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______}
    }
};

bool process_record_user(uint16_t keycode, keyrecord_t *record) {
    if (keycode >= MACRO_CODE_MIN && keycode <= MACRO_CODE_MAX && record->event.pressed) {
        process_macro_code(keycode);
    } else if (keycode >= FUNCTION_KEY_MIN && keycode <= FUNCTION_KEY_MAX && record->event.pressed) {
        process_function_key(keycode);
    }

    return true;
}

#endif

