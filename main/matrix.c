/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 *
 * Copyright 2018 Gal Zaidenstein.
 */

//GPIO libraries
#include "esp_system.h"
#include "driver/gpio.h"
#include "driver/rtc_io.h"
#include "keyboard_config.h"
#include "esp_sleep.h"
#include "esp_log.h"
#include "config.h"

#define GPIO_TAG "gpio"

const gpio_num_t rows[] = MATRIX_ROW_PINS;
const gpio_num_t cols[] = MATRIX_COL_PINS;

// deinitializing rtc matrix pins on  deep sleep wake up
void rtc_matrix_deinit(void) {

	// Deinitializing columns
	for (uint8_t col = 0; col < MATRIX_COLS; col++) {

		if (rtc_gpio_is_valid_gpio(cols[col]) == 1) {
			rtc_gpio_set_level(cols[col], 0);
			rtc_gpio_set_direction(cols[col],
					RTC_GPIO_MODE_DISABLED);
			gpio_reset_pin(cols[col]);
		}
	}

	// Deinitializing rows
	for (uint8_t row = 0; row < MATRIX_ROWS; row++) {

		if (rtc_gpio_is_valid_gpio(rows[row]) == 1) {
			rtc_gpio_set_level(rows[row], 0);
			rtc_gpio_set_direction(rows[row],
					RTC_GPIO_MODE_DISABLED);
			gpio_reset_pin(rows[row]);
		}
	}
}

// Initializing rtc matrix pins for deep sleep wake up
void rtc_matrix_setup(void) {
	uint64_t rtc_mask = 0;

	// Initializing columns
	for (uint8_t col = 0; col < MATRIX_COLS; col++) {

		if (rtc_gpio_is_valid_gpio(cols[col]) == 1) {
			rtc_gpio_init((cols[col]));
			rtc_gpio_set_direction(cols[col],
					RTC_GPIO_MODE_INPUT_OUTPUT);
			rtc_gpio_set_level(cols[col], 1);

			ESP_LOGI(GPIO_TAG,"%d is level %d", cols[col],
					gpio_get_level(cols[col]));
		}
	}

	// Initializing rows
	for (uint8_t row = 0; row < MATRIX_ROWS; row++) {

		if (rtc_gpio_is_valid_gpio(rows[row]) == 1) {
			rtc_gpio_init((rows[row]));
			rtc_gpio_set_direction(rows[row],
					RTC_GPIO_MODE_INPUT_OUTPUT);
			rtc_gpio_set_drive_capability(rows[row],
					GPIO_DRIVE_CAP_0);
			rtc_gpio_set_level(rows[row], 0);
			rtc_gpio_wakeup_enable(rows[row], GPIO_INTR_HIGH_LEVEL);
			SET_BIT(rtc_mask, rows[row]);

			ESP_LOGI(GPIO_TAG,"%d is level %d", rows[row],
					gpio_get_level(rows[row]));
		}
		esp_sleep_enable_ext1_wakeup(rtc_mask, ESP_EXT1_WAKEUP_ANY_HIGH);
	}
}

// Initializing matrix pins
void matrix_setup(void) {

#ifdef COL2ROW
	// Initializing columns
	for (uint8_t col = 0; col < MATRIX_COLS; col++) {

		gpio_pad_select_gpio(cols[col]);
		gpio_set_direction(cols[col], GPIO_MODE_INPUT_OUTPUT);
		gpio_set_level(cols[col], 0);

		ESP_LOGI(GPIO_TAG,"%d is level %d", cols[col],
				gpio_get_level(cols[col]));
	}

	// Initializing rows
	for (uint8_t row = 0; row < MATRIX_ROWS; row++) {

		gpio_pad_select_gpio(rows[row]);
		gpio_set_direction(rows[row], GPIO_MODE_INPUT_OUTPUT);
		gpio_set_drive_capability(rows[row], GPIO_DRIVE_CAP_0);
		gpio_set_level(rows[row], 0);

		ESP_LOGI(GPIO_TAG,"%d is level %d", rows[row],
				gpio_get_level(rows[row]));
	}
#endif
#ifdef ROW2COL
	// Initializing rows
	for(uint8_t row=0; row < MATRIX_ROWS; row++) {

		gpio_pad_select_gpio(rows[row]);
		gpio_set_direction(rows[row], GPIO_MODE_INPUT_OUTPUT);
		gpio_set_level(rows[row], 0);
		ESP_LOGI(GPIO_TAG,"%d is level %d",rows[row],gpio_get_level(rows[row]));
	}

	// Initializing columns
	for(uint8_t col=0; col < MATRIX_COLS; col++) {

		gpio_pad_select_gpio(cols[col]);
		gpio_set_direction(cols[col], GPIO_MODE_INPUT_OUTPUT);
		gpio_set_drive_capability(cols[col],GPIO_DRIVE_CAP_0);
		gpio_set_level(cols[col], 0);

		ESP_LOGI(GPIO_TAG,"%d is level %d",cols[col],gpio_get_level(cols[col]));
	}
#endif
}
