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
#include "esp_gap_ble_api.h"


enum passkey_event_e {
    PASSKEY_CHALLENGE = 0,  // Passkey challenge event
    PASSKEY_FAILURE,        // Passkey failure event
    PASSKEY_SUCCESS,        // Passkey success event
};

typedef void (*ble_passkey_callback)(void* arg, enum passkey_event_e event, esp_bd_addr_t bd_addr);

/** @brief Initialize BLE device and start HID interface
 *
 * Initializes the BT controller, Bluedroid stack, HID device, and custom services.
 * Can be called multiple times safely (will return immediately if already initialized).
 *
 * @param adv_name The advertising name for the BLE device
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t init_ble_device(const char *adv_name);

/** @brief Deinitialize BLE device and free resources
 *
 * Properly shuts down the BLE stack to allow dynamic on/off control.
 * Can be called multiple times safely (will return immediately if not initialized).
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t deinit_ble_device(void);

esp_err_t ble_gap_adv_to_any(const char* adv_name);

void ble_gap_set_passkey_callback(ble_passkey_callback callback, void* arg);

bool is_ble_ready();

#endif /* _HAL_BLE_H_ */
