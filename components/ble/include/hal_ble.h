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
 * Modified by: panhao356@gmail.com
 */

/** @file
 * @brief This file is a wrapper for the BLE-HID example of Espressif.
 */
#ifndef _HAL_BLE_H_
#define _HAL_BLE_H_

#include <stdbool.h>
#include <esp_err.h>
#include "esp_gap_ble_api.h"

// Forward declaration
struct esp_hidd_dev_s;

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

/** @brief Set passkey callback for BLE pairing
 *
 * Registers a callback function to handle passkey events during BLE pairing.
 *
 * @param callback The callback function to handle passkey events
 * @param arg User-defined argument passed to the callback
 */
void ble_gap_set_passkey_callback(ble_passkey_callback callback, void* arg);

/** @brief Check if BLE is initialized and ready
 *
 * Returns the initialization status of the BLE device.
 *
 * @return true if BLE is initialized and ready, false otherwise
 */
bool is_ble_ready(void);

/** @brief Get the HID device handle
 *
 * Returns the esp_hidd_dev_t handle for accessing connection information.
 *
 * @return esp_hidd_dev_t* pointer, or NULL if BLE is not initialized
 */
struct esp_hidd_dev_s* ble_get_hid_dev(void);

/** @brief Set BLE advertising speed
 *
 * Reconfigures and restarts BLE advertising with either fast or slow interval.
 * Fast: 50-100ms (for initial connection/discovery)
 * Slow: 500-1000ms (for idle power saving while still discoverable)
 *
 * @param slow true for slow advertising (power saving), false for fast
 * @return ESP_OK on success, ESP_ERR_INVALID_STATE if BLE not initialized
 */
esp_err_t ble_set_adv_speed(bool slow);

/** @brief Update BLE connection parameters for all active connections
 *
 * Requests connection interval update for every connected device.
 * Fast: 20-40ms (active typing)
 * Slow: 100-200ms (idle, saves power while staying connected)
 *
 * @param fast true for fast parameters (active use), false for slow (idle)
 * @return ESP_OK on success, ESP_ERR_INVALID_STATE if BLE not initialized
 */
esp_err_t ble_update_conn_params(bool fast);

#endif /* _HAL_BLE_H_ */
