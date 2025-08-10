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

#pragma once

#include <stdbool.h>
#include "esp_err.h"

//#define TEST_HARDWARE
#ifdef TEST_HARDWARE
#define LCD_WIDTH           280
#define LCD_HEIGHT          240
#else
// LCD Configuration
#define LCD_WIDTH           284
#define LCD_HEIGHT          76
#endif

/**
 * @brief Initialize ST7789 LCD hardware
 * @return esp_err_t ESP_OK on success
 */
esp_err_t lcd_hardware_init(void);

/**
 * @brief Set LCD backlight brightness
 * @param brightness Brightness level (0-100)
 * @return esp_err_t ESP_OK on success
 */
esp_err_t lcd_hardware_set_backlight(uint8_t brightness);

/**
 * @brief Turn LCD display on/off
 * @param on true to turn on, false to turn off
 * @return esp_err_t ESP_OK on success
 */
esp_err_t lcd_hardware_display_on_off(bool on);

/**
 * @brief Deinitialize LCD hardware
 * @return esp_err_t ESP_OK on success
 */
esp_err_t lcd_hardware_deinit(void);
