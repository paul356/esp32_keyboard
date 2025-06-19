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
 * Original Work: Copyright 2019 Benjamin Aigner <beni@asterics-foundation.org>
 * Modified by: github.com/paul356
 */

/** @file
 * @brief This file is a wrapper for the BLE-HID example of Espressif.
 */
#ifndef _HAL_BLE_H_
#define _HAL_BLE_H_

#include <stdbool.h>
#include <esp_err.h>

/** @brief Main init function to start HID interface (C interface)
 * @see hid_ble */
esp_err_t halBLEInit(const char* name);

bool is_ble_ready();

#endif /* _HAL_BLE_H_ */
