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

#pragma once

/**
 * @brief Input events for menu navigation
 */
typedef enum {
    INPUT_EVENT_NONE = 0,
    INPUT_EVENT_ENCODER_CW,  // Rotate clockwise
    INPUT_EVENT_ENCODER_CCW, // Rotate counter-clockwise
    INPUT_EVENT_KEYCODE,     // Press Enter
    INPUT_EVENT_TIMEOUT,     // Timeout to return to keyboard mode
    INPUT_EVENT_ENTER,       // Enter key
    INPUT_EVENT_ESC,         // Escape key
    INPUT_EVENT_BACKSPACE,   // Backspace key
    INPUT_EVENT_TAB,         // Tab key
    INPUT_EVENT_RIGHT_ARROW, // Navigate right in menu
    INPUT_EVENT_LEFT_ARROW,  // Navigate left in menu
    INPUT_EVENT_DOWN_ARROW,  // Navigate up in menu
    INPUT_EVENT_UP_ARROW,    // Navigate down in menu
    INPUT_EVENT_MAX
} input_event_e;