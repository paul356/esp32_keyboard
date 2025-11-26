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
 * Copyright 2025 github.com/paul356
 */

#include <string.h>
#include "drv_loop.h"
#include "esp_log.h"
#include "esp_event.h"
#include "ble_device.h"
#include "ble_events_prv.h"
#include "ble_gap.h"
#include "hid_desc.h"
#include "hal_ble.h"

#define BLE_EVENTS_POST_TIMEOUT pdMS_TO_TICKS(50)

static const char* TAG = "ble_events";

// BLE Event data structures
typedef struct {
    uint8_t battery_level;
} ble_battery_event_t;

typedef struct {
    uint8_t report_data[MAX_REPORT_LEN];  // Standard HID keyboard report size
    size_t report_len;
    bool nkro;
} ble_keyboard_event_t;

// BLE Event IDs
enum {
    BLE_BATTERY_UPDATE_EVENT,     // Battery level update event
    BLE_KEYBOARD_REPORT_EVENT,    // Keyboard report event
    BLE_CLEAR_BONDS_EVENT,        // Clear all bonded devices event
};

// BLE event base definition
ESP_EVENT_DEFINE_BASE(BLE_EVENTS);

// Forward declarations for event handlers
static void battery_event_handler(void* handler_args, esp_event_base_t base, int32_t id, void* event_data);
static void keyboard_event_handler(void* handler_args, esp_event_base_t base, int32_t id, void* event_data);
static void clear_bonds_event_handler(void* handler_args, esp_event_base_t base, int32_t id, void* event_data);

esp_err_t ble_post_battery_event(uint8_t battery_level)
{
    ble_battery_event_t event_data = {
        .battery_level = battery_level
    };

    esp_err_t err = drv_loop_post_event(BLE_EVENTS, BLE_BATTERY_UPDATE_EVENT, &event_data, sizeof(event_data),
                                        BLE_EVENTS_POST_TIMEOUT);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Fail to post battery events timeout: %s", esp_err_to_name(err));
    }
    return err;
}

esp_err_t ble_post_keyboard_event(const uint8_t* report_data, size_t report_len, bool nkro_bits)
{
    if (!report_data || report_len == 0 || report_len > sizeof(((ble_keyboard_event_t*)0)->report_data)) {
        ESP_LOGE(TAG, "Invalid report data parameters");
        return ESP_ERR_INVALID_ARG;
    }

    ble_keyboard_event_t event_data = {
        .report_len = report_len,
        .nkro = nkro_bits
    };
    memcpy(event_data.report_data, report_data, report_len);

    esp_err_t err = drv_loop_post_event(BLE_EVENTS, BLE_KEYBOARD_REPORT_EVENT, &event_data, sizeof(event_data),
                                        BLE_EVENTS_POST_TIMEOUT);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Fail to post keyboard events timeout: %s", esp_err_to_name(err));
    }
    return err;
}

esp_err_t ble_post_clear_bonds_event(void)
{
    esp_err_t err = drv_loop_post_event(BLE_EVENTS, BLE_CLEAR_BONDS_EVENT, NULL, 0,
                                        BLE_EVENTS_POST_TIMEOUT);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Fail to post clear bonds event timeout: %s", esp_err_to_name(err));
    }
    return err;
}

esp_err_t ble_events_init(void)
{
    ESP_LOGI(TAG, "Initializing BLE events");

    // Register event handlers using the generic drv_loop interface
    ESP_ERROR_CHECK(drv_loop_register_handler(BLE_EVENTS, BLE_BATTERY_UPDATE_EVENT,
                                             battery_event_handler, NULL));
    ESP_ERROR_CHECK(drv_loop_register_handler(BLE_EVENTS, BLE_KEYBOARD_REPORT_EVENT,
                                             keyboard_event_handler, NULL));
    ESP_ERROR_CHECK(drv_loop_register_handler(BLE_EVENTS, BLE_CLEAR_BONDS_EVENT,
                                             clear_bonds_event_handler, NULL));

    ESP_LOGI(TAG, "BLE event handlers registered");
    return ESP_OK;
}

// Event handlers - these are now internal to the BLE component
static void battery_event_handler(void* handler_args, esp_event_base_t base, int32_t id, void* event_data) {
    ble_battery_event_t* battery_data = (ble_battery_event_t*)event_data;

    if (!is_ble_ready()) {
        ESP_LOGW(TAG, "Skip BLE battery level update because ble is turned off.");
        return;
    }

    esp_hidd_dev_t* hid_dev = ble_get_hid_dev();
    if (!hid_dev || !esp_hidd_dev_connected(hid_dev))
        return;

    uint8_t battery_lev = battery_data->battery_level;
    esp_hidd_dev_battery_set(hid_dev, battery_lev);
    ESP_LOGI(TAG, "Battery level updated: %d", battery_lev);
}

static void keyboard_event_handler(void* handler_args, esp_event_base_t base, int32_t id, void* event_data) {
    ble_keyboard_event_t* kbd_data = (ble_keyboard_event_t*)event_data;

    if (!is_ble_ready()) {
        ESP_LOGW(TAG, "Skip BLE keyboard report because ble is turned off.");
        return;
    }

    esp_hidd_dev_t* hid_dev = ble_get_hid_dev();
    if (!hid_dev || !esp_hidd_dev_connected(hid_dev))
        return;

    // Hard code the positions of reports in the report maps
    if (kbd_data->nkro) {
        esp_hidd_dev_input_set(hid_dev, 1, REPORT_ID_NKRO, kbd_data->report_data, kbd_data->report_len);
    } else {
        // If it is 6nkro, use the failback report id in the report maps
        esp_hidd_dev_input_set(hid_dev, 0, REPORT_ID_KEYBOARD, kbd_data->report_data, kbd_data->report_len);
    }
}

static void clear_bonds_event_handler(void* handler_args, esp_event_base_t base, int32_t id, void* event_data) {
    ESP_LOGI(TAG, "Clearing all bonded BLE devices");

    if (!is_ble_ready()) {
        ESP_LOGW(TAG, "Skip clearing bonds because BLE is turned off.");
        return;
    }

    esp_err_t ret = ble_clear_all_bonds();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to clear bonded devices: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "Successfully cleared all bonded devices");
    }
}
