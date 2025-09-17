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
#include <string.h>
#include "hal_ble.h"
#include "esp_hidd.h"
#include "esp_check.h"
#include "ble_gap.h"
#include "layout_service.h"  // Include the layout service header
#include "ble_device.h"
#include "ble_events_prv.h"
#include "nvs_io.h"  // For NVS operations
#include "nvs.h"
#include "function_control.h"
#include "hid_desc.h"

#define TAG "hal_ble"

#define NVS_NAMESPACE "ble_device"

#define REPORT_ID_KEYBOARD 0x1
#define REPORT_LEN 8

uint8_t battery_report[1] = { 0 };
uint8_t key_report[REPORT_LEN] = { 0 };

static esp_hidd_dev_t* hid_dev;
static bool ble_ready = false;

// Make hid_dev accessible to other BLE modules
esp_hidd_dev_t* ble_get_hid_dev(void) {
    return hid_dev;
}

static esp_hid_raw_report_map_t report_maps[HID_REPORT_DESC_NUM];

static esp_hid_device_config_t ble_hid_config = {
    .vendor_id          = 0x16C0,
    .product_id         = 0x05DF,
    .version            = 0x0100,
    .device_name        = "smart_keyboard",
    .manufacturer_name  = "Paradise Lab",
    .serial_number      = "1234567890"
};

static void ble_hidd_event_callback(void *handler_args, esp_event_base_t base, int32_t id, void *event_data)
{
    esp_hidd_event_t event = (esp_hidd_event_t)id;
    esp_hidd_event_data_t *param = (esp_hidd_event_data_t *)event_data;

    switch (event) {
    case ESP_HIDD_START_EVENT: {
        break;
    }
    case ESP_HIDD_CONNECT_EVENT: {
        break;
    }
    case ESP_HIDD_PROTOCOL_MODE_EVENT: {
        ESP_LOGI(TAG, "PROTOCOL MODE[%u]: %s", param->protocol_mode.map_index, param->protocol_mode.protocol_mode ? "REPORT" : "BOOT");
        break;
    }
    case ESP_HIDD_CONTROL_EVENT: {
        ESP_LOGI(TAG, "CONTROL[%u]: %sSUSPEND", param->control.map_index, param->control.control ? "EXIT_" : "");
        break;
    }
    case ESP_HIDD_OUTPUT_EVENT: {
        ESP_LOGI(TAG, "OUTPUT[%u]: %8s ID: %2u, Len: %d, Data:", param->output.map_index,
                 esp_hid_usage_str(param->output.usage), param->output.report_id, param->output.length);
        ESP_LOG_BUFFER_HEX(TAG, param->output.data, param->output.length);

        // Set keyboard LED e.g Capslock, Numlock etc...
        if (param->output.report_id == REPORT_ID_KEYBOARD)
        {
            // bufsize should be (at least) 1
            if (param->output.length < 1)
                return;

            uint8_t const kbd_leds = param->output.data[0];
            set_caps_state((kbd_leds & 0x2) != 0);
        }
        break;
    }
    case ESP_HIDD_FEATURE_EVENT: {
        ESP_LOGI(TAG, "FEATURE[%u]: %8s ID: %2u, Len: %d, Data:", param->feature.map_index, esp_hid_usage_str(param->feature.usage), param->feature.report_id, param->feature.length);
        ESP_LOG_BUFFER_HEX(TAG, param->feature.data, param->feature.length);
        break;
    }
    case ESP_HIDD_DISCONNECT_EVENT: {
        ESP_LOGI(TAG, "DISCONNECT: %s", esp_hid_disconnect_reason_str(esp_hidd_dev_transport_get(param->disconnect.dev), param->disconnect.reason));
        break;
    }
    case ESP_HIDD_STOP_EVENT: {
        ESP_LOGI(TAG, "STOP");
        break;
    }
    default:
        break;
    }
    return;
}

bool is_ble_ready()
{
    return ble_ready;
}

/**
 * @brief Intermediary GATTS event handler
 * Routes events between ESP-IDF HID and our custom layout service
 */
void top_gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
{
    // Check if this is a registration event for our custom service
    if ((event == ESP_GATTS_REG_EVT && param->reg.app_id == LAYOUT_SERVICE_UUID) ||
        (event != ESP_GATTS_REG_EVT && gatts_if != ESP_GATT_IF_NONE && gatts_if == layout_service_get_gatts_if()))
    {
        if (event == ESP_GATTS_CONNECT_EVT)
        {
            const char* ble_name = get_ble_name();
            esp_err_t ret = ble_gap_adv_to_any(ble_name);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "Failed to start advertising for bonded devices: %s", esp_err_to_name(ret));
            }
        }

        // Pass to our layout service handler
        ESP_LOGI(TAG, "GATTS event: %d, GATTS interface: %d, to custom", event, gatts_if);
        layout_service_event_handler(event, gatts_if, param);
        return;
    }

    ESP_LOGI(TAG, "GATTS event: %d, GATTS interface: %d, to keyboard", event, gatts_if);
    // For all other events, pass to ESP-IDF HID handler
    esp_hidd_gatts_event_handler(event, gatts_if, param);
}

/*
 * CONTROLLER INIT
 * */

static esp_err_t init_bt_controller(uint8_t mode)
{
    esp_err_t ret;
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
#if CONFIG_IDF_TARGET_ESP32
    bt_cfg.mode = mode;
#endif
    {
        ret = esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);
        if (ret) {
            ESP_LOGE(TAG, "esp_bt_controller_mem_release failed: %d", ret);
            return ret;
        }
    }
    ret = esp_bt_controller_init(&bt_cfg);
    if (ret) {
        ESP_LOGE(TAG, "esp_bt_controller_init failed: %d", ret);
        return ret;
    }

    ret = esp_bt_controller_enable(mode);
    if (ret) {
        ESP_LOGE(TAG, "esp_bt_controller_enable failed: %d", ret);
        return ret;
    }

    esp_bluedroid_config_t bluedroid_cfg = BT_BLUEDROID_INIT_CONFIG_DEFAULT();
#if (CONFIG_EXAMPLE_SSP_ENABLED == false)
    bluedroid_cfg.ssp_en = false;
#endif
    ret = esp_bluedroid_init_with_cfg(&bluedroid_cfg);
    if (ret) {
        ESP_LOGE(TAG, "esp_bluedroid_init failed: %d", ret);
        return ret;
    }

    ret = esp_bluedroid_enable();
    if (ret) {
        ESP_LOGE(TAG, "esp_bluedroid_enable failed: %d", ret);
        return ret;
    }

    return ret;
}

/** @brief Main init function to start HID interface (C interface)
 * @see hid_ble */
esp_err_t halBLEInit(const char *adv_name)
{
    ESP_LOGI(TAG, "setting hid gap, mode:%d", ESP_BT_MODE_BLE);
    esp_err_t ret = init_bt_controller(ESP_BT_MODE_BLE);
    ESP_ERROR_CHECK( ret );

    if ((ret = esp_ble_gap_register_callback(ble_gap_event_handler)) != ESP_OK) {
        ESP_LOGE(TAG, "esp_ble_gap_register_callback failed: %d", ret);
        return ret;
    }

    // Register our intermediary GATTS event handler instead of ESP-IDF's handler directly
    if ((ret = esp_ble_gatts_register_callback(top_gatts_event_handler)) != ESP_OK) {
        ESP_LOGE(TAG, "GATTS register callback failed: %d", ret);
        return ret;
    }

    ESP_LOGI(TAG, "setting hid service");
    const uint8_t* starts[HID_REPORT_DESC_NUM];
    size_t len[HID_REPORT_DESC_NUM];
    get_hid_report_desc(starts, len, HID_REPORT_DESC_NUM);
    for (int i = 0; i < HID_REPORT_DESC_NUM; i++)
    {
        report_maps[i].data = starts[i];
        report_maps[i].len = len[i];
    }

    ble_hid_config.report_maps = &report_maps[0];
    ble_hid_config.report_maps_len = HID_REPORT_DESC_NUM;
    ESP_ERROR_CHECK(
        esp_hidd_dev_init(&ble_hid_config, ESP_HID_TRANSPORT_BLE, ble_hidd_event_callback, &hid_dev));

    // Initialize the custom layout service
    ESP_LOGI(TAG, "setting custom service");
    ret = layout_service_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize layout service: %s", esp_err_to_name(ret));
        return ret;
    }

    // Initialize BLE-specific events and register handlers
    ret = ble_events_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize BLE events: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = ble_gap_set_sec_params();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set BLE security parameters: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = ble_gap_adv_to_any(adv_name);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "GAP adv_start failed: %d", ret);
    }

    ESP_LOGI(TAG, "BLE event handlers registered");

    //set log level according to define
    esp_log_level_set(TAG, ESP_LOG_INFO);
    ble_ready = true;

    return ESP_OK;
}
