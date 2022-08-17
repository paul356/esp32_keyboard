#include "keycode_conv.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "keymap.h"
#include "key_definitions.h"

#define TAG "KEY TO CHAR"

#define CHAR_ILLEGAL     0xff
#define CHAR_RETURN     '\n'
#define CHAR_ESCAPE      27
#define CHAR_TAB         '\t'
#define CHAR_BACKSPACE   0x7f

// Simplified US Keyboard with Shift modifier, taken from btstack's hid example

/**
 * English (US)
 */
static const uint8_t keytable_us_none[] = {
CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL, /*   0-3 */
'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', /*  4-13 */
'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't', /* 14-23 */
'u', 'v', 'w', 'x', 'y', 'z', /* 24-29 */
'1', '2', '3', '4', '5', '6', '7', '8', '9', '0', /* 30-39 */
CHAR_RETURN, CHAR_ESCAPE, CHAR_BACKSPACE, CHAR_TAB, ' ', /* 40-44 */
'-', '=', '[', ']', '\\', CHAR_ILLEGAL, ';', '\'', 0x60, ',', /* 45-54 */
'.', '/', CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL, /* 55-60 */
CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL, /* 61-64 */
CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL, /* 65-68 */
CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL, /* 69-72 */
CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL, /* 73-76 */
CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL, /* 77-80 */
CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL, /* 81-84 */
'*', '-', '+', '\n', '1', '2', '3', '4', '5', /* 85-97 */
'6', '7', '8', '9', '0', '.', 0xa7, /* 97-100 */
};

static const uint8_t keytable_us_shift[] = {
CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL, /*  0-3  */
'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', /*  4-13 */
'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', /* 14-23 */
'U', 'V', 'W', 'X', 'Y', 'Z', /* 24-29 */
'!', '@', '#', '$', '%', '^', '&', '*', '(', ')', /* 30-39 */
CHAR_RETURN, CHAR_ESCAPE, CHAR_BACKSPACE, CHAR_TAB, ' ', /* 40-44 */
'_', '+', '{', '}', '|', CHAR_ILLEGAL, ':', '"', 0x7E, '<', /* 45-54 */
'>', '?', CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL, /* 55-60 */
CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL, /* 61-64 */
CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL, /* 65-68 */
CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL, /* 69-72 */
CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL, /* 73-76 */
CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL, /* 77-80 */
CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL, /* 81-84 */
'*', '-', '+', '\n', '1', '2', '3', '4', '5', /* 85-97 */
'6', '7', '8', '9', '0', '.', 0xb1, /* 97-100 */
};

void suspend_ble(void) {
	BLE_EN = 0;
	ESP_LOGI(TAG, "Suspending hid reports");

}

void enable_ble(void) {
	BLE_EN = 1;
	ESP_LOGI(TAG, "Resuming hid reports");

}

/*
 * @brief retrieve chars corresponding to keycode
 */
char keycode_to_char(uint16_t keycode, uint8_t modifier) {

	char key_ascii = CHAR_ILLEGAL;
	if ((CHECK_BIT(modifier, 1) != 0) || (CHECK_BIT(modifier, 2) != 0)) {

		if (keycode <= 100) {
			key_ascii = keytable_us_shift[keycode];
		}
	}

	else {
		if (keycode <= 100) {
			key_ascii = keytable_us_none[keycode];
		}
	}
	return key_ascii;
}

/*
 * @brief retrieve keycode corresponding to char
 */
uint8_t char_to_keycode(char ascii_key) {

	uint8_t keycode = CHAR_ILLEGAL;

	for (int i = 0; i < 100; i++) {
		if ((ascii_key == keytable_us_none[i])
				|| (ascii_key == keytable_us_shift[i])) {
			keycode = i;

		}
	}

	return keycode;
}
