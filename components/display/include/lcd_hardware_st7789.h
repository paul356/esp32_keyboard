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

#include "esp_err.h"
#include "esp_lcd_types.h"
#include "lvgl.h"

// LCD Configuration
#define LCD_WIDTH           284
#define LCD_HEIGHT          76
#define LCD_BIT_PER_PIXEL   16

// GPIO Pin Definitions
#define LCD_PIN_MOSI        11
#define LCD_PIN_CLK         12
#define LCD_PIN_CS          10
#define LCD_PIN_DC          13
#define LCD_PIN_RST         9
#define LCD_PIN_BL          14

// SPI Configuration
#define LCD_SPI_HOST        SPI2_HOST
#define LCD_PIXEL_CLOCK_HZ  (20 * 1000 * 1000)  // 20MHz

/**
 * @brief LCD hardware interface structure
 */
typedef struct {
    esp_lcd_panel_handle_t panel_handle;
    esp_lcd_panel_io_handle_t io_handle;
    lv_display_t *lvgl_display;
    bool initialized;
} lcd_hardware_t;

/**
 * @brief Initialize ST7789 LCD hardware
 * @return esp_err_t ESP_OK on success
 */
esp_err_t lcd_hardware_init(void);

/**
 * @brief Get LCD hardware instance
 * @return lcd_hardware_t* Pointer to LCD hardware instance
 */
lcd_hardware_t* lcd_hardware_get_instance(void);

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
