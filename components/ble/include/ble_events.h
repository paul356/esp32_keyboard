/*
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

#ifdef __cplusplus
}
#endif

#endif // BLE_EVENTS_H
