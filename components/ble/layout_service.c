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

/** @file
 * @brief Implementation of BLE custom service for keyboard layout and keycode configuration.
 */

#include "layout_service.h"
#include "keymap_json.h"
#include "drv_loop.h"
#include "esp_event.h"
#include <string.h>

#define TAG "layout_service"

// Define task stack size and priority
#define LAYOUT_UPDATE_TASK_STACK_SIZE 4096
#define LAYOUT_UPDATE_TASK_PRIORITY 5
#define LAYOUT_UPDATE_QUEUE_SIZE 3

// Define BLE default MTU size (23 bytes according to BLE specification)
#define BLE_DEFAULT_MTU_SIZE 23
#define BLE_MAX_ATTR_LEN 512 // Maximum attribute length for responses

// Helper macros for min/max
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define MAX(a, b) (((a) > (b)) ? (a) : (b))

typedef struct {
    uint8_t* data;          // Pointer to the JSON data
    uint16_t data_len;      // Current data length
    uint16_t max_len;       // Maximum allocation size
} layout_json_data_t;

// Layout Event data structures
typedef struct {
    uint8_t* json_data;
    size_t data_len;
} layout_update_event_t;

enum {
    LAYOUT_UPDATE_EVENT = 0, // Event for layout updates
} layout_event_id_t;

// Service handles
static uint16_t layout_service_handle = 0;
static uint16_t layout_char_handle = 0;
static uint16_t layout_offset_char_handle = 0;
static uint16_t keycode_char_handle = 0;
static uint16_t keycode_offset_char_handle = 0;
static uint16_t layout_update_char_handle = 0;  // New handle for layout update characteristic

// GATTS profile interface
static esp_gatt_if_t layout_service_gatts_if = 0;
static uint16_t layout_service_conn_id = 0;
static bool layout_service_connected = false;
static uint16_t layout_service_mtu = BLE_DEFAULT_MTU_SIZE; // Store the negotiated MTU

// JSON data buffers
static layout_json_data_t layout_query_data = {0};
static layout_json_data_t keycode_query_data = {0};
static layout_json_data_t layout_update_data = {0}; // Buffer for incoming layout update data

// Offset values
static uint16_t layout_query_offset = 0;
static uint16_t keycode_query_offset = 0;

ESP_EVENT_DEFINE_BASE(LAYOUT_EVENTS);

static bool process_layout_update_data(layout_json_data_t *json_data);

static esp_err_t layout_post_update_event(uint8_t* json_data, size_t data_len)
{
    if (!json_data || data_len == 0) {
        ESP_LOGE(TAG, "Invalid JSON data parameters");
        return ESP_ERR_INVALID_ARG;
    }

    layout_update_event_t event_data = {
        .json_data = json_data,
        .data_len = data_len
    };

    esp_err_t err = drv_loop_post_event(LAYOUT_EVENTS,
                                       LAYOUT_UPDATE_EVENT,
                                       &event_data, sizeof(event_data),
                                       portMAX_DELAY);

    if (err != ESP_OK) {
        // Free the allocated memory if posting failed
        ESP_LOGE(TAG, "Failed to post layout event: %s", esp_err_to_name(err));
        free(json_data);
    }

    return err;
}

static void layout_event_handler(void* handler_args, esp_event_base_t base, int32_t id, void* event_data) {
    layout_update_event_t* layout_data = (layout_update_event_t*)event_data;

    ESP_LOGI(TAG, "Processing layout update in event handler, data length: %d", layout_data->data_len);
    ESP_LOGI(TAG, "JSON data: %s", (char*)layout_data->json_data);

    cJSON* json_data = cJSON_Parse((char*)(layout_data->json_data));

    // Free the allocated data immediately after parsing
    free(layout_data->json_data);
    layout_data->json_data = NULL;

    if (json_data == NULL) {
        ESP_LOGE(TAG, "Failed to parse JSON data");
    } else {
        // Process the JSON data (e.g., update keymaps, layouts, etc.)
        esp_err_t err = update_layout_from_json(json_data);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to update layout from JSON: %d", err);
        } else {
            ESP_LOGI(TAG, "Layout updated successfully from JSON");
        }

        cJSON_Delete(json_data); // Clean up after processing
    }
}

static esp_err_t layout_events_init(void)
{
    ESP_LOGI(TAG, "Initializing layout events");

    // Register event handler using the generic drv_loop interface
    ESP_ERROR_CHECK(drv_loop_register_handler(LAYOUT_EVENTS, LAYOUT_UPDATE_EVENT,
                                             layout_event_handler, NULL));

    ESP_LOGI(TAG, "Layout event handler registered");
    return ESP_OK;
}

/**
 * @brief Event handler for layout updates
 *
 * This handler processes layout update data received via events
 * without blocking the BLE stack.
 *
 * @param handler_args Handler arguments (unused)
 * @param base Event base
 * @param id Event ID
 * @param event_data Event data containing layout update information
 */
/**
 * Helper function to append a string to the JSON buffer
 */
static void ble_append_str(void *target, const char *str)
{
    if (!target || !str)
    {
        return;
    }

    layout_json_data_t *json_data = (layout_json_data_t *)target;
    size_t str_len = strlen(str);

    // Check if there's enough space
    if (json_data->data_len + str_len >= json_data->max_len)
    {
        // Buffer is full, try to resize
        uint16_t new_size = json_data->max_len * 2;
        if (new_size < json_data->data_len + str_len + 1)
        {
            new_size = json_data->data_len + str_len + 128; // Add some extra space
        }

        uint8_t *new_buf = realloc(json_data->data, new_size);
        if (new_buf)
        {
            json_data->data = new_buf;
            json_data->max_len = new_size;
        }
        else
        {
            ESP_LOGE(TAG, "Failed to resize JSON buffer");
            return; // Cannot append more data
        }
    }

    // Copy the data
    memcpy(json_data->data + json_data->data_len, str, str_len);
    json_data->data_len += str_len;
    json_data->data[json_data->data_len] = '\0'; // Ensure null termination
}

/**
 * @brief Get the GATTS interface for the layout service
 *
 * @return esp_gatt_if_t The GATTS interface
 */
esp_gatt_if_t layout_service_get_gatts_if(void)
{
    return layout_service_gatts_if;
}

void layout_service_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
{
    ESP_LOGD(TAG, "Layout service event: %d", event);

    switch (event)
    {
    case ESP_GATTS_REG_EVT:
        if (param->reg.status == ESP_GATT_OK)
        {
            layout_service_gatts_if = gatts_if;

            // Create the custom keyboard configuration service with primary service
            esp_gatt_srvc_id_t service_id;
            service_id.id.inst_id = 0;
            service_id.id.uuid.len = ESP_UUID_LEN_16;
            service_id.id.uuid.uuid.uuid16 = LAYOUT_SERVICE_UUID;
            service_id.is_primary = true;

            esp_err_t ret = esp_ble_gatts_create_service(gatts_if, &service_id, 16); // 16 handles should be enough
            if (ret != ESP_OK)
            {
                ESP_LOGE(TAG, "Create service failed: %d", ret);
            }
        }
        else
        {
            ESP_LOGE(TAG, "Custom service register failed, status %d", param->reg.status);
        }
        break;

    case ESP_GATTS_CREATE_EVT:
        if (param->create.status == ESP_GATT_OK)
        {
            layout_service_handle = param->create.service_handle;

            // Start the service
            esp_ble_gatts_start_service(layout_service_handle);

            // Add layout characteristic
            esp_bt_uuid_t layout_uuid;
            layout_uuid.len = ESP_UUID_LEN_16;
            layout_uuid.uuid.uuid16 = LAYOUT_CHAR_UUID;

            esp_err_t ret = esp_ble_gatts_add_char(
                layout_service_handle, &layout_uuid, ESP_GATT_PERM_READ,
                ESP_GATT_CHAR_PROP_BIT_READ, NULL, NULL);

            if (ret != ESP_OK)
            {
                ESP_LOGE(TAG, "Add char failed: %d", ret);
            }
        }
        else
        {
            ESP_LOGE(TAG, "Create service failed, status %d", param->create.status);
        }
        break;

    case ESP_GATTS_ADD_CHAR_EVT:
        if (param->add_char.status == ESP_GATT_OK)
        {
            if (param->add_char.char_uuid.uuid.uuid16 == LAYOUT_CHAR_UUID)
            {
                layout_char_handle = param->add_char.attr_handle;

                // Add layout offset characteristic
                esp_bt_uuid_t layout_offset_uuid;
                layout_offset_uuid.len = ESP_UUID_LEN_16;
                layout_offset_uuid.uuid.uuid16 = LAYOUT_OFFSET_CHAR_UUID;

                esp_ble_gatts_add_char(layout_service_handle, &layout_offset_uuid,
                                      ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
                                      ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_WRITE,
                                      NULL, NULL);
            }
            else if (param->add_char.char_uuid.uuid.uuid16 == LAYOUT_OFFSET_CHAR_UUID)
            {
                layout_offset_char_handle = param->add_char.attr_handle;

                // Now add the keycode characteristic
                esp_bt_uuid_t keycode_uuid;
                keycode_uuid.len = ESP_UUID_LEN_16;
                keycode_uuid.uuid.uuid16 = KEYCODE_CHAR_UUID;

                esp_ble_gatts_add_char(layout_service_handle, &keycode_uuid, ESP_GATT_PERM_READ,
                                       ESP_GATT_CHAR_PROP_BIT_READ,
                                       NULL, NULL);
            }
            else if (param->add_char.char_uuid.uuid.uuid16 == KEYCODE_CHAR_UUID)
            {
                keycode_char_handle = param->add_char.attr_handle;

                // Add keycode offset characteristic
                esp_bt_uuid_t keycode_offset_uuid;
                keycode_offset_uuid.len = ESP_UUID_LEN_16;
                keycode_offset_uuid.uuid.uuid16 = KEYCODE_OFFSET_CHAR_UUID;

                esp_ble_gatts_add_char(layout_service_handle, &keycode_offset_uuid,
                                      ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
                                      ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_WRITE,
                                      NULL, NULL);
            }
            else if (param->add_char.char_uuid.uuid.uuid16 == KEYCODE_OFFSET_CHAR_UUID)
            {
                keycode_offset_char_handle = param->add_char.attr_handle;
                ESP_LOGI(TAG, "Keycode offset characteristic added, handle: %d", keycode_offset_char_handle);

                // Now add layout update characteristic (write-only)
                esp_bt_uuid_t layout_update_uuid;
                layout_update_uuid.len = ESP_UUID_LEN_16;
                layout_update_uuid.uuid.uuid16 = LAYOUT_UPDATE_CHAR_UUID;

                esp_ble_gatts_add_char(layout_service_handle, &layout_update_uuid,
                                      ESP_GATT_PERM_WRITE,  // Write-only permission
                                      ESP_GATT_CHAR_PROP_BIT_WRITE,
                                      NULL, NULL);
            }
            else if (param->add_char.char_uuid.uuid.uuid16 == LAYOUT_UPDATE_CHAR_UUID)
            {
                layout_update_char_handle = param->add_char.attr_handle;
                ESP_LOGI(TAG, "Layout update characteristic added, handle: %d", layout_update_char_handle);
            }
        }
        else
        {
            ESP_LOGE(TAG, "Add characteristic failed, status %d", param->add_char.status);
        }
        break;

    case ESP_GATTS_CONNECT_EVT:
        layout_service_conn_id = param->connect.conn_id;
        layout_service_connected = true;
        break;

    case ESP_GATTS_DISCONNECT_EVT:
        layout_service_connected = false;
        break;

    case ESP_GATTS_MTU_EVT:
        // Store the negotiated MTU for use in read responses
        layout_service_mtu = param->mtu.mtu;
        ESP_LOGI(TAG, "MTU negotiated: %d", layout_service_mtu);
        break;

    case ESP_GATTS_READ_EVT:
        if (param->read.handle == layout_char_handle)
        {
            esp_gatt_rsp_t rsp = {0};
            if (param->read.offset != 0) {
                rsp.attr_value.len = 0;
                rsp.attr_value.handle = param->read.handle;
                rsp.attr_value.offset = param->read.offset;
                esp_ble_gatts_send_response(gatts_if, param->read.conn_id, param->read.trans_id, ESP_GATT_OK, &rsp);
                break;
            }

            if (layout_query_data.data == NULL)
            {
                // Initialize the layout query data buffer if not already done
                layout_query_data.max_len = LAYOUT_JSON_MAX_SIZE;
                layout_query_data.data = malloc(layout_query_data.max_len);
                layout_query_data.data_len = 0;
                if (!layout_query_data.data)
                {
                    ESP_LOGE(TAG, "Failed to allocate memory for layout query data");
                    esp_ble_gatts_send_response(gatts_if, param->read.conn_id, param->read.trans_id, ESP_GATT_INSUF_RESOURCE, NULL);
                    break;
                }
            }

            // Generate the JSON using our utility function
            generate_layouts_json(ble_append_str, &layout_query_data);

            // Use the negotiated MTU or default if not set yet
            uint16_t mtu = layout_service_mtu > 0 ? layout_service_mtu : BLE_DEFAULT_MTU_SIZE;

            // Use the offset from the characteristic
            uint16_t effective_offset = layout_query_offset;
            uint16_t available_len = 0;

            if (effective_offset < layout_query_data.data_len) {
                available_len = layout_query_data.data_len - effective_offset;
            }

            uint16_t send_len = MIN(available_len, MIN(BLE_MAX_ATTR_LEN, mtu - 1)); // MTU minus 1-byte opcode

            if (effective_offset >= layout_query_data.data_len)
            {
                // Offset is beyond the data length, send empty response
                send_len = 0;
            }

            ESP_LOGI(TAG, "Layout read request, stored offset: %d, request offset: %d, effective offset: %d, available: %d, sending: %d",
                     layout_query_offset, param->read.offset, effective_offset, available_len, send_len);

            rsp.attr_value.len = send_len;
            rsp.attr_value.handle = param->read.handle;
            rsp.attr_value.offset = 0;

            if (send_len > 0)
            {
                memcpy(rsp.attr_value.value, layout_query_data.data + effective_offset, send_len);
            }

            // Send response
            esp_err_t ret =
                esp_ble_gatts_send_response(gatts_if, param->read.conn_id, param->read.trans_id, ESP_GATT_OK, &rsp);
            if (ret != ESP_OK)
            {
                ESP_LOGE(TAG, "Send response failed: %d", ret);
            }

            free(layout_query_data.data);
            layout_query_data.data = NULL; // Free the buffer after sending
            layout_query_data.data_len = 0; // Reset the length
        }
        else if (param->read.handle == keycode_char_handle)
        {
            esp_gatt_rsp_t rsp = {0};
            if (param->read.offset != 0) {
                rsp.attr_value.len = 0;
                rsp.attr_value.handle = param->read.handle;
                rsp.attr_value.offset = param->read.offset;
                esp_ble_gatts_send_response(gatts_if, param->read.conn_id, param->read.trans_id, ESP_GATT_OK, &rsp);
                break;
            }

            if (keycode_query_data.data == NULL)
            {
                // Initialize the layout query data buffer if not already done
                keycode_query_data.max_len = LAYOUT_JSON_MAX_SIZE;
                keycode_query_data.data = malloc(keycode_query_data.max_len);
                keycode_query_data.data_len = 0;
                if (!keycode_query_data.data)
                {
                    ESP_LOGE(TAG, "Failed to allocate memory for keycode query data");
                    esp_ble_gatts_send_response(gatts_if, param->read.conn_id, param->read.trans_id, ESP_GATT_INSUF_RESOURCE, NULL);
                    break;
                }
            }

            // Generate the JSON using our utility function
            generate_keycodes_json(ble_append_str, &keycode_query_data);

            // Use the negotiated MTU or default if not set yet
            uint16_t mtu = layout_service_mtu > 0 ? layout_service_mtu : BLE_DEFAULT_MTU_SIZE;

            // Use the offset from the characteristic
            uint16_t effective_offset = keycode_query_offset;
            uint16_t available_len = 0;

            if (effective_offset < keycode_query_data.data_len) {
                available_len = keycode_query_data.data_len - effective_offset;
            }

            uint16_t send_len = MIN(available_len, MIN(BLE_MAX_ATTR_LEN, mtu - 1)); // MTU minus 1-byte opcode

            if (effective_offset >= keycode_query_data.data_len)
            {
                // Offset is beyond the data length, send empty response
                send_len = 0;
            }

            ESP_LOGI(TAG, "Keycode read request, stored offset: %d, request offset: %d, effective offset: %d, available: %d, sending: %d",
                     keycode_query_offset, param->read.offset, effective_offset, available_len, send_len);

            rsp.attr_value.len = send_len;
            rsp.attr_value.handle = param->read.handle;
            rsp.attr_value.offset = 0;

            if (send_len > 0)
            {
                memcpy(rsp.attr_value.value, keycode_query_data.data + effective_offset, send_len);
            }

            // Send response
            esp_err_t ret =
                esp_ble_gatts_send_response(gatts_if, param->read.conn_id, param->read.trans_id, ESP_GATT_OK, &rsp);
            if (ret != ESP_OK)
            {
                ESP_LOGE(TAG, "Send response failed: %d", ret);
            }

            free(keycode_query_data.data);
            keycode_query_data.data_len = 0; // Reset the length
            keycode_query_data.data = NULL; // Free the buffer after sending
        }
        else if (param->read.handle == layout_offset_char_handle)
        {
            // Handle layout offset read request
            esp_gatt_rsp_t rsp = {0};

            rsp.attr_value.handle = param->read.handle;
            rsp.attr_value.len = 2; // 16-bit value (2 bytes)
            rsp.attr_value.offset = 0;

            // Set the value (little-endian)
            rsp.attr_value.value[0] = layout_query_offset & 0xFF;
            rsp.attr_value.value[1] = (layout_query_offset >> 8) & 0xFF;

            ESP_LOGI(TAG, "Layout offset read: %d", layout_query_offset);

            // Send response
            esp_ble_gatts_send_response(gatts_if, param->read.conn_id, param->read.trans_id, ESP_GATT_OK, &rsp);
        }
        else if (param->read.handle == keycode_offset_char_handle)
        {
            // Handle keycode offset read request
            esp_gatt_rsp_t rsp = {0};

            rsp.attr_value.handle = param->read.handle;
            rsp.attr_value.len = 2; // 16-bit value (2 bytes)
            rsp.attr_value.offset = 0;

            // Set the value (little-endian)
            rsp.attr_value.value[0] = keycode_query_offset & 0xFF;
            rsp.attr_value.value[1] = (keycode_query_offset >> 8) & 0xFF;

            ESP_LOGI(TAG, "Keycode offset read: %d", keycode_query_offset);

            // Send response
            esp_ble_gatts_send_response(gatts_if, param->read.conn_id, param->read.trans_id, ESP_GATT_OK, &rsp);
        }
        break;

    case ESP_GATTS_WRITE_EVT:
        if (!param->write.is_prep)
        {
            // This is a complete write operation (not a prepare write)
            if (param->write.handle == layout_offset_char_handle)
            {
                // Process layout offset write
                if (param->write.len >= 2)
                {
                    // Extract 16-bit offset value (little-endian)
                    uint16_t new_offset = param->write.value[0] | (param->write.value[1] << 8);

                    // Always allow setting the offset - even if it's beyond the data length
                    layout_query_offset = new_offset;
                    ESP_LOGI(TAG, "Layout offset set to: %d (data length: %d)",
                             layout_query_offset, layout_query_data.data ? layout_query_data.data_len : 0);
                }
            }
            else if (param->write.handle == keycode_offset_char_handle)
            {
                // Process keycode offset write
                if (param->write.len >= 2)
                {
                    // Extract 16-bit offset value (little-endian)
                    uint16_t new_offset = param->write.value[0] | (param->write.value[1] << 8);

                    // Always allow setting the offset - even if it's beyond the data length
                    keycode_query_offset = new_offset;
                    ESP_LOGI(TAG, "Keycode offset set to: %d (data length: %d)",
                             keycode_query_offset, keycode_query_data.data ? keycode_query_data.data_len : 0);
                }
            }
            else if (param->write.handle == layout_update_char_handle)
            {
                // Process layout update write (JSON data from client)
                ESP_LOGI(TAG, "Received layout update data, length: %d", param->write.len);

                if (layout_update_data.data == NULL)
                {
                    // Allocate initial buffer if not already done
                    layout_update_data.max_len = LAYOUT_JSON_MAX_SIZE;
                    layout_update_data.data = malloc(layout_update_data.max_len);
                    layout_update_data.data_len = 0;
                    if (layout_update_data.data == NULL)
                    {
                        ESP_LOGE(TAG, "Failed to allocate memory for layout update data");
                        if (param->write.need_rsp)
                        {
                            esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id,
                                                        ESP_GATT_INSUF_RESOURCE, NULL);
                            break; // Skip the standard response below
                        }
                    }
                }

                // Check if we have enough space in our buffer
                if (layout_update_data.data_len + param->write.len <= layout_update_data.max_len)
                {
                    // Append the new data to our buffer
                    memcpy(layout_update_data.data + layout_update_data.data_len,
                           param->write.value,
                           param->write.len);
                    layout_update_data.data_len += param->write.len;

                    // Ensure null-termination for JSON processing
                    layout_update_data.data[layout_update_data.data_len] = '\0';

                    ESP_LOGI(TAG, "Updated layout data buffer, total length: %d", layout_update_data.data_len);

                    if (layout_update_data.data_len > 0) {

                        // Process the complete layout update data
                        process_layout_update_data(&layout_update_data);
                    }
                }
                else
                {
                    ESP_LOGE(TAG, "Layout update buffer overflow, discarding update");
                    // Reset the buffer for next update
                    layout_update_data.data_len = 0;
                    if (param->write.need_rsp)
                    {
                        esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id,
                                                    ESP_GATT_INVALID_ATTR_LEN, NULL);
                        break; // Skip the standard response below
                    }
                }
            }
            // If an attempt is made to write to read-only characteristics,
            // just ignore it or respond with an error if response is needed
            else if (param->write.handle == layout_char_handle || param->write.handle == keycode_char_handle)
            {
                ESP_LOGW(TAG, "Attempt to write to read-only characteristic: %d", param->write.handle);
                // Respond with permission error if response is needed
                if (param->write.need_rsp)
                {
                    esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id,
                                                ESP_GATT_WRITE_NOT_PERMIT, NULL);
                    break; // Skip the standard response below
                }
            }

            // Send response
            if (param->write.need_rsp)
            {
                esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, ESP_GATT_OK, NULL);
            }
        } else {
            // This is a prepare write operation (for data larger than MTU)
            if (param->write.handle == layout_update_char_handle)
            {
                ESP_LOGI(TAG, "Prepare write for layout update, offset: %d, length: %d",
                         param->write.offset, param->write.len);

                if (layout_update_data.data == NULL)
                {
                    // Allocate initial buffer if not already done
                    layout_update_data.max_len = LAYOUT_JSON_MAX_SIZE;
                    layout_update_data.data = malloc(layout_update_data.max_len);
                    layout_update_data.data_len = 0;
                    if (layout_update_data.data == NULL)
                    {
                        ESP_LOGE(TAG, "Failed to allocate memory for layout update data");
                        esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id,
                                                    ESP_GATT_INSUF_RESOURCE, NULL);
                        break; // Skip the standard response below
                    }
                }

                // Check if the offset is valid and we have enough buffer space
                if (param->write.offset + param->write.len <= layout_update_data.max_len)
                {
                    // For prepare writes, we store data at the specified offset
                    // This allows the client to send data out of order if needed
                    if (param->write.offset + param->write.len > layout_update_data.data_len)
                    {
                        layout_update_data.data_len = param->write.offset + param->write.len;
                    }

                    // Copy data to the buffer at the specified offset
                    memcpy(layout_update_data.data + param->write.offset, param->write.value, param->write.len);

                    // Prepare writes always need a response (as per BLE specification)
                    esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, ESP_GATT_OK, NULL);
                }
                else
                {
                    ESP_LOGE(TAG, "Prepare write buffer overflow, offset: %d, len: %d, max: %d",
                             param->write.offset, param->write.len, layout_update_data.max_len);
                    // Prepare writes always need a response with appropriate error code
                    esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id,
                                               ESP_GATT_PREPARE_Q_FULL, NULL);
                }
            }
            else
            {
                // Prepare writes not supported for other characteristics
                // Prepare writes always need a response
                esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id,
                                           ESP_GATT_WRITE_NOT_PERMIT, NULL);
            }
        }
        break;

    case ESP_GATTS_EXEC_WRITE_EVT:
        // Execute or cancel prepared writes
        if (param->exec_write.exec_write_flag == ESP_GATT_PREP_WRITE_EXEC)
        {
            // Execute prepared writes
            ESP_LOGI(TAG, "Executing prepared writes for layout update data");

            // Process the complete layout update data that was received through prepare writes
            if (layout_update_data.data_len > 0)
            {
                // Ensure null-termination for JSON processing
                layout_update_data.data[layout_update_data.data_len] = '\0';

                ESP_LOGI(TAG, "Received complete layout update JSON data, length: %d", layout_update_data.data_len);

                // Process the data using our shared helper function
                process_layout_update_data(&layout_update_data);
            }
        }
        else
        {
            // Cancel prepared writes
            ESP_LOGI(TAG, "Cancelling prepared writes for layout update data");
            layout_update_data.data_len = 0;  // Reset buffer if writes are cancelled
        }

        // Send response for execute write request
        esp_ble_gatts_send_response(gatts_if, param->exec_write.conn_id, param->exec_write.trans_id, ESP_GATT_OK, NULL);
        break;

    default:
        break;
    }
}

esp_err_t layout_service_init(void)
{
    // Initialize with default JSON data
    if (!layout_query_data.data)
    {
        layout_query_data.data = NULL;
        layout_query_data.max_len = LAYOUT_JSON_MAX_SIZE;
        layout_query_data.data_len = 0;
    }

    if (!keycode_query_data.data)
    {
        keycode_query_data.data = NULL;
        keycode_query_data.max_len = LAYOUT_JSON_MAX_SIZE;
        keycode_query_data.data_len = 0;
    }

    // Initialize the layout update buffer
    layout_update_data.data = NULL;
    layout_update_data.max_len = LAYOUT_JSON_MAX_SIZE;
    layout_update_data.data_len = 0;

    // Initialize offset values
    layout_query_offset = 0;
    keycode_query_offset = 0;

    // Initialize the generic event system and register layout event handler
    esp_err_t err = layout_events_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize layout events: %s", esp_err_to_name(err));
        return err;
    }
    ESP_LOGI(TAG, "Layout event handler registered");

    // Register custom service application
    return esp_ble_gatts_app_register(LAYOUT_SERVICE_UUID);
}

/**
 * @brief Process complete layout update data and queue it for handling
 *
 * This function creates a copy of the layout update data and sends it to
 * the processing queue. It's used for both regular and execute write events.
 *
 * @param data Pointer to the data buffer
 * @param data_len Length of the data
 * @return true if successfully queued, false otherwise
 */
static bool process_layout_update_data(layout_json_data_t *json_data)
{
    if (json_data->data == NULL || json_data->data_len == 0) {
        ESP_LOGE(TAG, "Invalid layout update data");
        return false;
    }

    ESP_LOGI(TAG, "Processing layout update data, length: %d", json_data->data_len);

    // Post layout event instead of using queue
    esp_err_t ret = layout_post_update_event(json_data->data, json_data->data_len);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to post layout event: %d", ret);
        return false;
    }

    json_data->data = NULL;
    json_data->data_len = 0; // Reset the data after queuing
    ESP_LOGI(TAG, "Layout update queued for processing");
    return true;
}