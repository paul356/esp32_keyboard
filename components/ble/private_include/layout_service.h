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
 */

/** @file
 * @brief This file provides BLE custom service for keyboard layout and keycode configuration.
 */
#ifndef _LAYOUT_SERVICE_H_
#define _LAYOUT_SERVICE_H_

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

#include "esp_err.h"
#include "esp_log.h"

#include "esp_bt.h"
#include "esp_bt_defs.h"
#include "esp_bt_main.h"
#include "esp_gap_bt_api.h"
#include "esp_gatt_defs.h"
#include "esp_gattc_api.h"
#include "esp_gatts_api.h"
#include "esp_hid_common.h"

#ifdef __cplusplus
extern "C" {
#endif

// Maximum size for JSON data
#define LAYOUT_JSON_MAX_SIZE  2048
#define LAYOUT_CHUNK_SIZE     512

// UUIDs for the custom service and characteristics
#define LAYOUT_SERVICE_UUID      0x181C  // Custom service UUID
#define LAYOUT_CHAR_UUID         0xDEF1  // Layout characteristic UUID
#define LAYOUT_OFFSET_CHAR_UUID  0xDEF2  // Layout offset characteristic UUID
#define KEYCODE_CHAR_UUID        0xDEF3  // Keycode characteristic UUID
#define KEYCODE_OFFSET_CHAR_UUID 0xDEF4  // Keycode offset characteristic UUID

/**
 * @brief Initialize the layout service
 */
void layout_service_init(void);

/**
 * @brief GATT event handler for the layout service
 *
 * @param event GATT event
 * @param gatts_if GATT interface
 * @param param Event parameters
 */
void layout_service_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);

/** @brief Get the GATTS interface for the layout service
 * @return GATTS interface handle
 */
esp_gatt_if_t layout_service_get_gatts_if(void);

#ifdef __cplusplus
}
#endif

#endif /* _LAYOUT_SERVICE_H_ */