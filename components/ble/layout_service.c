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
 * @brief Implementation of BLE custom service for keyboard layout and keycode configuration.
 */

#include <string.h>
#include "layout_service.h"

#define TAG "layout_service"

// Service handles
static uint16_t layout_service_handle = 0;
static uint16_t layout_char_handle = 0;
static uint16_t layout_descr_handle = 0;
static uint16_t keycode_char_handle = 0;
static uint16_t keycode_descr_handle = 0;

// GATTS profile interface
static esp_gatt_if_t layout_service_gatts_if = 0;
static uint16_t layout_service_conn_id = 0;
static bool layout_service_connected = false;

// JSON data buffers
static layout_json_data_t layout_data = {0};
static layout_json_data_t keycode_data = {0};

// Default JSON content
static const char* default_layout_json = "{\"layout\":\"qwerty\"}";
static const char* default_keycode_json = "{\"keycodes\":[]}";

// Helper macros for min/max
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define MAX(a, b) (((a) > (b)) ? (a) : (b))

/**
 * @brief Get the GATTS interface for the layout service
 *
 * @return esp_gatt_if_t The GATTS interface
 */
esp_gatt_if_t layout_service_get_gatts_if(void)
{
    return layout_service_gatts_if;
}

// This adapter function bridges between the old function name used in ESP-IDF
// and our new implementation name
void custom_service_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
{
    layout_service_event_handler(event, gatts_if, param);
}

void layout_service_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
{
    ESP_LOGD(TAG, "Layout service event: %d", event);

    switch (event) {
        case ESP_GATTS_REG_EVT:
            if (param->reg.status == ESP_GATT_OK) {
                layout_service_gatts_if = gatts_if;

                // Create the custom keyboard configuration service with primary service
                esp_gatt_srvc_id_t service_id;
                service_id.id.inst_id = 0;
                service_id.id.uuid.len = ESP_UUID_LEN_16;
                service_id.id.uuid.uuid.uuid16 = LAYOUT_SERVICE_UUID;
                service_id.is_primary = true;

                esp_err_t ret = esp_ble_gatts_create_service(gatts_if, &service_id, 16); // 16 handles should be enough
                if (ret != ESP_OK) {
                    ESP_LOGE(TAG, "Create service failed: %d", ret);
                }
            } else {
                ESP_LOGE(TAG, "Custom service register failed, status %d", param->reg.status);
            }
            break;

        case ESP_GATTS_CREATE_EVT:
            if (param->create.status == ESP_GATT_OK) {
                layout_service_handle = param->create.service_handle;

                // Start the service
                esp_ble_gatts_start_service(layout_service_handle);

                // Add layout characteristic
                esp_bt_uuid_t layout_uuid;
                layout_uuid.len = ESP_UUID_LEN_16;
                layout_uuid.uuid.uuid16 = LAYOUT_CHAR_UUID;

                esp_err_t ret = esp_ble_gatts_add_char(layout_service_handle, &layout_uuid,
                                          ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
                                          ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_WRITE | ESP_GATT_CHAR_PROP_BIT_NOTIFY,
                                          NULL, NULL);

                if (ret != ESP_OK) {
                    ESP_LOGE(TAG, "Add char failed: %d", ret);
                }
            } else {
                ESP_LOGE(TAG, "Create service failed, status %d", param->create.status);
            }
            break;

        case ESP_GATTS_ADD_CHAR_EVT:
            if (param->add_char.status == ESP_GATT_OK) {
                if (param->add_char.char_uuid.uuid.uuid16 == LAYOUT_CHAR_UUID) {
                    layout_char_handle = param->add_char.attr_handle;

                    // Add Client Characteristic Configuration descriptor for notifications
                    esp_bt_uuid_t layout_descr_uuid;
                    layout_descr_uuid.len = ESP_UUID_LEN_16;
                    layout_descr_uuid.uuid.uuid16 = ESP_GATT_UUID_CHAR_CLIENT_CONFIG;

                    esp_ble_gatts_add_char_descr(layout_service_handle, &layout_descr_uuid,
                                               ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE, NULL, NULL);

                    // Now add the keycode characteristic
                    esp_bt_uuid_t keycode_uuid;
                    keycode_uuid.len = ESP_UUID_LEN_16;
                    keycode_uuid.uuid.uuid16 = KEYCODE_CHAR_UUID;

                    esp_ble_gatts_add_char(layout_service_handle, &keycode_uuid,
                                         ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
                                         ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_WRITE | ESP_GATT_CHAR_PROP_BIT_NOTIFY,
                                         NULL, NULL);
                }
                else if (param->add_char.char_uuid.uuid.uuid16 == KEYCODE_CHAR_UUID) {
                    keycode_char_handle = param->add_char.attr_handle;

                    // Add Client Characteristic Configuration descriptor for notifications
                    esp_bt_uuid_t keycode_descr_uuid;
                    keycode_descr_uuid.len = ESP_UUID_LEN_16;
                    keycode_descr_uuid.uuid.uuid16 = ESP_GATT_UUID_CHAR_CLIENT_CONFIG;

                    esp_ble_gatts_add_char_descr(layout_service_handle, &keycode_descr_uuid,
                                               ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE, NULL, NULL);
                }
            } else {
                ESP_LOGE(TAG, "Add characteristic failed, status %d", param->add_char.status);
            }
            break;

        case ESP_GATTS_ADD_CHAR_DESCR_EVT:
            if (param->add_char_descr.status == ESP_GATT_OK) {
                if (layout_descr_handle == 0) {
                    layout_descr_handle = param->add_char_descr.attr_handle;
                } else {
                    keycode_descr_handle = param->add_char_descr.attr_handle;
                }
            } else {
                ESP_LOGE(TAG, "Add descriptor failed, status %d", param->add_char_descr.status);
            }
            break;

        case ESP_GATTS_CONNECT_EVT:
            layout_service_conn_id = param->connect.conn_id;
            layout_service_connected = true;
            break;

        case ESP_GATTS_DISCONNECT_EVT:
            layout_service_connected = false;
            break;

        case ESP_GATTS_READ_EVT:
            if (param->read.handle == layout_char_handle) {
                // Handle layout read request
                esp_gatt_rsp_t rsp = {0};
                rsp.attr_value.len = layout_data.data_len;
                rsp.attr_value.handle = param->read.handle;
                rsp.attr_value.offset = param->read.offset;

                // Calculate data to send based on the offset
                uint16_t copy_len = MIN(layout_data.data_len - param->read.offset, ESP_GATT_MAX_ATTR_LEN);
                memcpy(rsp.attr_value.value, layout_data.data + param->read.offset, copy_len);

                // Send response
                esp_ble_gatts_send_response(gatts_if, param->read.conn_id, param->read.trans_id, ESP_GATT_OK, &rsp);

                ESP_LOGI(TAG, "Layout read request, offset: %d, len: %d", param->read.offset, copy_len);
            }
            else if (param->read.handle == keycode_char_handle) {
                // Handle keycode read request
                esp_gatt_rsp_t rsp = {0};
                rsp.attr_value.len = keycode_data.data_len;
                rsp.attr_value.handle = param->read.handle;
                rsp.attr_value.offset = param->read.offset;

                // Calculate data to send based on the offset
                uint16_t copy_len = MIN(keycode_data.data_len - param->read.offset, ESP_GATT_MAX_ATTR_LEN);
                memcpy(rsp.attr_value.value, keycode_data.data + param->read.offset, copy_len);

                // Send response
                esp_ble_gatts_send_response(gatts_if, param->read.conn_id, param->read.trans_id, ESP_GATT_OK, &rsp);

                ESP_LOGI(TAG, "Keycode read request, offset: %d, len: %d", param->read.offset, copy_len);
            }
            break;

        case ESP_GATTS_WRITE_EVT:
            if (!param->write.is_prep) {
                // This is a complete write operation (not a prepare write)
                if (param->write.handle == layout_char_handle) {
                    ESP_LOGI(TAG, "Layout write, len: %d", param->write.len);

                    // Make sure we have enough memory
                    if (!layout_data.data) {
                        layout_data.data = (uint8_t*)malloc(LAYOUT_JSON_MAX_SIZE);
                        layout_data.max_len = LAYOUT_JSON_MAX_SIZE;
                    }

                    if (param->write.len <= LAYOUT_JSON_MAX_SIZE) {
                        memcpy(layout_data.data, param->write.value, param->write.len);
                        layout_data.data_len = param->write.len;
                        // Null-terminate the data to make it a valid string
                        layout_data.data[param->write.len] = '\0';

                        ESP_LOGI(TAG, "Layout updated: %s", (char*)layout_data.data);
                    }
                }
                else if (param->write.handle == keycode_char_handle) {
                    ESP_LOGI(TAG, "Keycode write, len: %d", param->write.len);

                    // Make sure we have enough memory
                    if (!keycode_data.data) {
                        keycode_data.data = (uint8_t*)malloc(LAYOUT_JSON_MAX_SIZE);
                        keycode_data.max_len = LAYOUT_JSON_MAX_SIZE;
                    }

                    if (param->write.len <= LAYOUT_JSON_MAX_SIZE) {
                        memcpy(keycode_data.data, param->write.value, param->write.len);
                        keycode_data.data_len = param->write.len;
                        // Null-terminate the data to make it a valid string
                        keycode_data.data[param->write.len] = '\0';

                        ESP_LOGI(TAG, "Keycode updated: %s", (char*)keycode_data.data);
                    }
                }
                else if (param->write.handle == layout_descr_handle || param->write.handle == keycode_descr_handle) {
                    ESP_LOGI(TAG, "CCCD write, handle: %d, value: %d", param->write.handle, param->write.value[0]);
                }

                // Send response
                if (param->write.need_rsp) {
                    esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, ESP_GATT_OK, NULL);
                }
            }
            else {
                // This is a prepare write operation for long writes
                if (param->write.handle == layout_char_handle) {
                    // Setup the prepared write if it's the first chunk
                    if (!layout_data.is_prepared) {
                        layout_data.is_prepared = true;
                        layout_data.prepared_len = 0;
                    }

                    // Make sure we have enough memory
                    if (!layout_data.data) {
                        layout_data.data = (uint8_t*)malloc(LAYOUT_JSON_MAX_SIZE);
                        layout_data.max_len = LAYOUT_JSON_MAX_SIZE;
                    }

                    // Check offset and length
                    if (param->write.offset + param->write.len <= LAYOUT_JSON_MAX_SIZE) {
                        memcpy(layout_data.data + param->write.offset, param->write.value, param->write.len);
                        layout_data.prepared_len = MAX(layout_data.prepared_len, param->write.offset + param->write.len);
                    }
                }
                else if (param->write.handle == keycode_char_handle) {
                    // Setup the prepared write if it's the first chunk
                    if (!keycode_data.is_prepared) {
                        keycode_data.is_prepared = true;
                        keycode_data.prepared_len = 0;
                    }

                    // Make sure we have enough memory
                    if (!keycode_data.data) {
                        keycode_data.data = (uint8_t*)malloc(LAYOUT_JSON_MAX_SIZE);
                        keycode_data.max_len = LAYOUT_JSON_MAX_SIZE;
                    }

                    // Check offset and length
                    if (param->write.offset + param->write.len <= LAYOUT_JSON_MAX_SIZE) {
                        memcpy(keycode_data.data + param->write.offset, param->write.value, param->write.len);
                        keycode_data.prepared_len = MAX(keycode_data.prepared_len, param->write.offset + param->write.len);
                    }
                }
            }
            break;

        case ESP_GATTS_EXEC_WRITE_EVT:
            // Execute or cancel prepared writes
            if (param->exec_write.exec_write_flag == ESP_GATT_PREP_WRITE_EXEC) {
                // Execute the prepared write
                if (layout_data.is_prepared) {
                    layout_data.data_len = layout_data.prepared_len;
                    layout_data.data[layout_data.data_len] = '\0'; // Null-terminate
                    ESP_LOGI(TAG, "Layout execute write, len: %d", layout_data.data_len);
                    ESP_LOGI(TAG, "Layout updated: %s", (char*)layout_data.data);
                }

                if (keycode_data.is_prepared) {
                    keycode_data.data_len = keycode_data.prepared_len;
                    keycode_data.data[keycode_data.data_len] = '\0'; // Null-terminate
                    ESP_LOGI(TAG, "Keycode execute write, len: %d", keycode_data.data_len);
                    ESP_LOGI(TAG, "Keycode updated: %s", (char*)keycode_data.data);
                }
            } else {
                // Cancel prepared writes
                ESP_LOGI(TAG, "Cancel prepared writes");
            }

            // Reset prepare flags
            layout_data.is_prepared = false;
            keycode_data.is_prepared = false;
            break;

        default:
            break;
    }
}

void layout_service_init(void) {
    // Initialize with default JSON data
    if (!layout_data.data) {
        layout_data.data = (uint8_t*)malloc(LAYOUT_JSON_MAX_SIZE);
        layout_data.max_len = LAYOUT_JSON_MAX_SIZE;
        size_t len = strlen(default_layout_json);
        memcpy(layout_data.data, default_layout_json, len);
        layout_data.data_len = len;
        layout_data.data[len] = '\0';
    }

    if (!keycode_data.data) {
        keycode_data.data = (uint8_t*)malloc(LAYOUT_JSON_MAX_SIZE);
        keycode_data.max_len = LAYOUT_JSON_MAX_SIZE;
        size_t len = strlen(default_keycode_json);
        memcpy(keycode_data.data, default_keycode_json, len);
        keycode_data.data_len = len;
        keycode_data.data[len] = '\0';
    }

    // Register custom service application
    esp_ble_gatts_app_register(LAYOUT_SERVICE_UUID);
}

const char* layout_service_get_layout_json(void) {
    if (layout_data.data) {
        return (const char*)layout_data.data;
    }
    return default_layout_json;
}

const char* layout_service_get_keycode_json(void) {
    if (keycode_data.data) {
        return (const char*)keycode_data.data;
    }
    return default_keycode_json;
}

esp_err_t layout_service_set_layout_json(const char* json_data) {
    if (!json_data) {
        return ESP_ERR_INVALID_ARG;
    }

    size_t len = strlen(json_data);
    if (len >= LAYOUT_JSON_MAX_SIZE) {
        ESP_LOGE(TAG, "JSON data too large");
        return ESP_ERR_NO_MEM;
    }

    if (!layout_data.data) {
        layout_data.data = (uint8_t*)malloc(LAYOUT_JSON_MAX_SIZE);
        layout_data.max_len = LAYOUT_JSON_MAX_SIZE;
    }

    memcpy(layout_data.data, json_data, len);
    layout_data.data_len = len;
    layout_data.data[len] = '\0';

    // Send notification if connected and notifications are enabled
    if (layout_service_connected && layout_char_handle > 0) {
        // Send notifications in chunks if data is larger than MTU
        uint16_t offset = 0;
        while (offset < layout_data.data_len) {
            uint16_t chunk_size = MIN(LAYOUT_CHUNK_SIZE, layout_data.data_len - offset);
            esp_ble_gatts_send_indicate(layout_service_gatts_if, layout_service_conn_id,
                                       layout_char_handle, chunk_size, layout_data.data + offset, false);
            offset += chunk_size;
        }
    }

    return ESP_OK;
}

esp_err_t layout_service_set_keycode_json(const char* json_data) {
    if (!json_data) {
        return ESP_ERR_INVALID_ARG;
    }

    size_t len = strlen(json_data);
    if (len >= LAYOUT_JSON_MAX_SIZE) {
        ESP_LOGE(TAG, "JSON data too large");
        return ESP_ERR_NO_MEM;
    }

    if (!keycode_data.data) {
        keycode_data.data = (uint8_t*)malloc(LAYOUT_JSON_MAX_SIZE);
        keycode_data.max_len = LAYOUT_JSON_MAX_SIZE;
    }

    memcpy(keycode_data.data, json_data, len);
    keycode_data.data_len = len;
    keycode_data.data[len] = '\0';

    // Send notification if connected and notifications are enabled
    if (layout_service_connected && keycode_char_handle > 0) {
        // Send notifications in chunks if data is larger than MTU
        uint16_t offset = 0;
        while (offset < keycode_data.data_len) {
            uint16_t chunk_size = MIN(LAYOUT_CHUNK_SIZE, keycode_data.data_len - offset);
            esp_ble_gatts_send_indicate(layout_service_gatts_if, layout_service_conn_id,
                                       keycode_char_handle, chunk_size, keycode_data.data + offset, false);
            offset += chunk_size;
        }
    }

    return ESP_OK;
}