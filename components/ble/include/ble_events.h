/*
 * Copyright (C) 2026 panhao356@gmail.com
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * ble_events.h - BLE component specific events
 *
 * This header defines events specific to the BLE component
 */

#ifndef BLE_EVENTS_H
#define BLE_EVENTS_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Post a battery update event
 *
 * @param battery_level Battery level (0-100)
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t ble_post_battery_event(uint8_t battery_level);

/**
 * @brief Post a keyboard report event
 *
 * @param report_data Pointer to keyboard report data
 * @param report_len Length of the report data
 * @param nkro_bits Data format is a bitmap of key range 0x0 - 0x7f
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t ble_post_keyboard_event(const uint8_t* report_data, size_t report_len, bool nkro_bits);

/**
 * @brief Post an event to clear all bonded BLE devices
 *
 * This function posts an event to the driver loop to clear all bonded devices.
 * The actual clearing happens asynchronously in the event handler.
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t ble_post_clear_bonds_event(void);

#ifdef __cplusplus
}
#endif

#endif // BLE_EVENTS_H
