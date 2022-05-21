#ifndef KEYMAP_C
#define KEYMAP_C

#include "key_definitions.h"
#include "keyboard_config.h"
#include "keymap.h"
#include "plugins.h"

// A bit different from QMK, default returns you to the first layer, LOWER and raise increase/lower layer by order.
#define DEFAULT 0x100
#define LOWER 0x101
#define RAISE 0x102

// Keymaps are designed to be relatively interchangeable with QMK
enum custom_keycodes {
	QWERTY, NUM,
    PLUGINS,
};

//Set these for each layer and use when layers are needed in a hold-to use layer
enum layer_holds {
	QWERTY_H = LAYER_HOLD_BASE_VAL, NUM_H,FUNCS_H
};

// array to hold names of layouts for oled
char default_layout_names[LAYERS][MAX_LAYOUT_NAME_LENGTH] = { "QWERTY", "NUM",
		  "Plugins",
		};

/* select a keycode for your macro
 * important - first macro must be initialized as MACRO_BASE_VAL
 * */

#define MACROS_NUM 2
enum custom_macros {
	KC_CTRL_ALT_DELETE = MACRO_BASE_VAL, KC_ALT_F4,
};

/*define what the macros do
 * important- make sure you you put the macros in the same order as the their enumeration
 */
uint16_t macros[MACROS_NUM][MACRO_LEN] = {
		// CTRL+ALT+DEL
		{ KC_LCTRL, KC_LALT, KC_DEL },
		//ALT +F4
		{ KC_RALT, KC_LALT, KC_NO } };

/*Encoder keys for each layer by order, and for each pad
 * First variable states what usage the encoder has
 */

uint16_t default_encoder_map[LAYERS][ENCODER_SIZE] = {
		// |VOL + | VOL - | MUTE |
		{ MEDIA_ENCODER, KC_AUDIO_VOL_UP, KC_AUDIO_VOL_DOWN, KC_AUDIO_MUTE },
		// |Y+|Y-| LEFT CLICK|
		{ MOUSE_ENCODER, KC_MS_UP, KC_MS_DOWN, KC_MS_BTN1 },

		{ MOUSE_ENCODER, KC_MS_UP, KC_MS_DOWN, KC_MS_BTN1 } };

uint16_t default_slave_encoder_map[LAYERS][ENCODER_SIZE] = {
		// |VOL + | VOL - | MUTE |
		{ MOUSE_ENCODER, KC_MS_WH_UP, KC_MS_WH_DOWN	, KC_AUDIO_MUTE },
		// |Y+|Y-| LEFT CLICK|
		{ MOUSE_ENCODER, KC_MS_RIGHT, KC_MS_LEFT, KC_MS_BTN2 },
		// |Y+|Y-| LEFT CLICK|
		{ MOUSE_ENCODER, KC_MS_RIGHT, KC_MS_LEFT, KC_MS_BTN2 } };

// Fillers to make layering more clear
#define _______ KC_TRNS
#define XXXXXXX KC_NO

//NOTE: For this keymap due to wiring constraints the the two last rows on the left are wired unconventionally
// Each keymap is represented by an array, with an array that points to all the keymaps  by order
uint16_t _QWERTY[MATRIX_ROWS][KEYMAP_COLS]={
    {KC_GRV,  KC_1,    KC_2,    KC_3,    KC_4,    KC_5   },
    {KC_ESC,  KC_Q,    KC_W,    KC_E,    KC_R,    KC_T   },
    {KC_TAB,  KC_A,    KC_S,    KC_D,    KC_F,    KC_G   },
    {KC_LSFT, KC_Z,    KC_X,    KC_C,    KC_V,    KC_B   },
    {KC_LCTRL,KC_LGUI, KC_LALT, KC_SPC,  KC_SPC,  NUM_H  },
    {DEFAULT, RAISE,   KC_NONE, KC_NONE, KC_NONE, KC_NONE}

};

uint16_t _NUM[MATRIX_ROWS][KEYMAP_COLS]={
    {KC_F1,   KC_F2,   KC_F3,   KC_F4,   KC_F5,   KC_F6  },
    {KC_ESC,  KC_Q,    KC_W,    KC_E,    KC_R,    KC_T   }, 
    {KC_TAB,  KC_A,    KC_S,    KC_D,    KC_F,    KC_G   }, 
    {KC_LSFT, KC_Z,    KC_X,    KC_C,    KC_V,    KC_B   }, 
    {KC_LCTRL,KC_LGUI, KC_LALT, KC_SPC,  KC_SPC,  NUM_H  }, 
    {DEFAULT, RAISE,   KC_NONE, KC_NONE, KC_NONE, KC_NONE}
};

uint16_t _PLUGINS[MATRIX_ROWS][KEYMAP_COLS]={
    {KC_GRV,  KC_1,    KC_2,    KC_3,    KC_4,    KC_5   },
    {KC_ESC,  KC_Q,    KC_W,    KC_E,    KC_R,    KC_T   },
    {KC_TAB,  KC_A,    KC_S,    KC_D,    KC_F,    KC_G   },
    {KC_LSFT, KC_Z,    KC_X,    KC_C,    KC_V,    KC_B   },
    {KC_LCTRL,KC_LGUI, KC_LALT, KC_SPC,  KC_SPC,  NUM_H  },
    {DEFAULT,RAISE, KC_NONE, KC_NONE, KC_NONE,   KC_NONE }
};
//Create an array that points to the various keymaps
uint16_t (*default_layouts[])[MATRIX_ROWS][KEYMAP_COLS] = { &_QWERTY, &_NUM,
                                                            &_PLUGINS
};

uint8_t current_layout = 0;

#endif

