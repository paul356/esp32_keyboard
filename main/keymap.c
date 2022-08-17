#ifndef KEYMAP_C
#define KEYMAP_C

#include "key_definitions.h"
#include "keyboard_config.h"
#include "keymap.h"

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
    "QWERTY", "NUM", "Plugins",
};

//NOTE: For this keymap due to wiring constraints the the two last rows on the left are wired unconventionally
// Each keymap is represented by an array, with an array that points to all the keymaps  by order
uint16_t _LAYERS[LAYERS][MATRIX_ROWS][MATRIX_COLS] = {
    {
        {KC_ESC,  KC_F1,   KC_F2,   KC_F3,   KC_F4,   KC_F5,   KC_F6,   KC_F7,   KC_F8,   KC_F9,   KC_F10,  KC_F11,  KC_F12,  KC_DEL},
        {KC_GRV,  KC_1,    KC_2,    KC_3,    KC_4,    KC_5,    KC_6,    KC_7,    KC_8,    KC_9,    KC_0,    KC_MINUS,KC_EQUAL,KC_BSPACE},
        {KC_TAB,  KC_Q,    KC_W,    KC_E,    KC_R,    KC_T,    KC_Y,    KC_U,    KC_I,    KC_O,    KC_P,    KC_LBRACKET,KC_RBRACKET,KC_BSLASH},
        {KC_CAPS, KC_A,    KC_S,    KC_D,    KC_F,    KC_G,    KC_H,    KC_J,    KC_K,    KC_L,    KC_SCOLON,KC_QUOTE,KC_ENTER,_______},
        {KC_LSHIFT,KC_Z,   KC_X,    KC_C,    KC_V,    KC_B,    KC_N,    KC_M,    KC_COMMA,KC_DOT,  KC_SLASH,KC_RSHIFT,KC_UP, _______},
        {KC_LCTRL,KC_LGUI, KC_LALT, KC_SPACE,_______, _______, KC_RALT, _______, KC_LCTRL,KC_LEFT, KC_DOWN, KC_RIGHT, KC_PGDOWN,KC_PGUP}
    },
    {
        {KC_ESC,  KC_F1,   KC_F2,   KC_F3,   KC_F4,   KC_F5,   KC_F6,   KC_F7,   KC_F8,   KC_F9,   KC_F10,  KC_F11,  KC_F12,  KC_DEL},
        {KC_GRV,  KC_1,    KC_2,    KC_3,    KC_4,    KC_5,    KC_6,    KC_7,    KC_8,    KC_9,    KC_0,    KC_MINUS,KC_EQUAL,KC_BSPACE},
        {KC_TAB,  KC_Q,    KC_W,    KC_E,    KC_R,    KC_T,    KC_Y,    KC_U,    KC_I,    KC_O,    KC_P,    KC_LBRACKET,KC_RBRACKET,KC_BSLASH},
        {KC_CAPS, KC_A,    KC_S,    KC_D,    KC_F,    KC_G,    KC_H,    KC_J,    KC_K,    KC_L,    KC_SCOLON,KC_QUOTE,KC_ENTER,_______},
        {KC_LSHIFT,KC_Z,   KC_X,    KC_C,    KC_V,    KC_B,    KC_N,    KC_M,    KC_COMMA,KC_DOT,  KC_SLASH,KC_RSHIFT,KC_UP, _______},
        {KC_LCTRL,KC_LGUI, KC_LALT, KC_SPACE,_______, _______, KC_RALT, _______, KC_LCTRL,KC_LEFT, KC_DOWN, KC_RIGHT, KC_PGDOWN,KC_PGUP}
    },
    {
        {KC_ESC,  KC_F1,   KC_F2,   KC_F3,   KC_F4,   KC_F5,   KC_F6,   KC_F7,   KC_F8,   KC_F9,   KC_F10,  KC_F11,  KC_F12,  KC_DEL},
        {KC_GRV,  KC_1,    KC_2,    KC_3,    KC_4,    KC_5,    KC_6,    KC_7,    KC_8,    KC_9,    KC_0,    KC_MINUS,KC_EQUAL,KC_BSPACE},
        {KC_TAB,  KC_Q,    KC_W,    KC_E,    KC_R,    KC_T,    KC_Y,    KC_U,    KC_I,    KC_O,    KC_P,    KC_LBRACKET,KC_RBRACKET,KC_BSLASH},
        {KC_CAPS, KC_A,    KC_S,    KC_D,    KC_F,    KC_G,    KC_H,    KC_J,    KC_K,    KC_L,    KC_SCOLON,KC_QUOTE,KC_ENTER,_______},
        {KC_LSHIFT,KC_Z,   KC_X,    KC_C,    KC_V,    KC_B,    KC_N,    KC_M,    KC_COMMA,KC_DOT,  KC_SLASH,KC_RSHIFT,KC_UP, _______},
        {KC_LCTRL,KC_LGUI, KC_LALT, KC_SPACE,_______, _______, KC_RALT, _______, KC_LCTRL,KC_LEFT, KC_DOWN, KC_RIGHT, KC_PGDOWN,KC_PGUP}
    }
};

uint8_t current_layout = 0;

#endif

