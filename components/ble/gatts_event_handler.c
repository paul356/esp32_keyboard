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
 * @brief This file provides an intermediary GATTS event handler that routes events
 * between the ESP-IDF HID event handler and our custom layout service handler.
 */

#include <string.h>
#include "esp_log.h"
#include "esp_gatts_api.h"
#include "layout_service.h"

#define TAG "gatts_handler"

// External declaration for the ESP-IDF HID GATTS event handler
extern void esp_hidd_gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);
extern esp_gatt_if_t layout_service_get_gatts_if(void);

/**
 * @brief Intermediary GATTS event handler
 * Routes events between ESP-IDF HID and our custom layout service
 */
void mk32_gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
{
    // Check if this is a registration event for our custom service
    if (event == ESP_GATTS_REG_EVT) {
        if (param->reg.app_id == LAYOUT_SERVICE_UUID) {
            // Pass to our layout service handler
            ESP_LOGI(TAG, "GATTS event: %d, GATTS interface: %d, to custom", event, gatts_if);
            layout_service_event_handler(event, gatts_if, param);
            return;
        }
    // For non-registration events, check if the interface matches our custom service
    } else if (gatts_if != ESP_GATT_IF_NONE && gatts_if == layout_service_get_gatts_if()) {
        // Get the GATTS interface for our layout service
        ESP_LOGI(TAG, "GATTS event: %d, GATTS interface: %d, to custom", event, gatts_if);
        layout_service_event_handler(event, gatts_if, param);
        return;
    }

    ESP_LOGI(TAG, "GATTS event: %d, GATTS interface: %d, to keyboard", event, gatts_if);
    // For all other events, pass to ESP-IDF HID handler
    esp_hidd_gatts_event_handler(event, gatts_if, param);
}