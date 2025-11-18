/*
 *  This program is a keyboard firmware for ESP family boards inspired by
 *  MK32 and qmk_firmware.
 *
 *  Copyright (C) 2025 github.com/paul356
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

//#define TEST_HARDWARE
#ifdef TEST_HARDWARE
#define LCD_WIDTH           280
#define LCD_HEIGHT          240
#else
// LCD Configuration
#define LCD_WIDTH           284
#define LCD_HEIGHT          76
#endif

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
#define DRAW_BUFFER_SIZE    (LCD_WIDTH * LCD_HEIGHT * sizeof(lv_color16_t) / 10)
#define SCREEN_OFFSET_X     18  // Offset for X coordinate, adjust as needed
#define SCREEN_OFFSET_Y     82