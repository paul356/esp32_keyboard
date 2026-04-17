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
 * Copyright 2025 panhao356@gmail.com
 */

/** @file
 * @brief Implementation of BLE custom service for keyboard layout and keycode configuration.
 */

#include "layout_service.h"
#include "keymap_json.h"
#include "drv_loop.h"
#include "esp_event.h"
#include "macros.h"
#include "cJSON.h"
#include <string.h>

#define TAG "layout_service"

#define LAYOUT_UPDATE_POST_TIMEOUT pdMS_TO_TICKS(50)

// Define BLE default MTU size (23 bytes according to BLE specification)
#define BLE_DEFAULT_MTU_SIZE 23
#define BLE_MAX_ATTR_LEN 512 // Maximum attribute length for responses

// Helper macros for min/max
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define MAX(a, b) (((a) > (b)) ? (a) : (b))

// Maximum number of concurrent connections supported
#define MAX_CONCURRENT_CONNECTIONS 9

typedef struct {
    uint8_t* data;          // Pointer to the JSON data
    uint16_t data_len;      // Current data length
    uint16_t max_len;       // Maximum allocation size
} layout_json_data_t;

// Per-connection context structure
typedef struct {
    uint16_t conn_id;                           // Connection ID
    bool in_use;                               // Whether this slot is active
    uint16_t mtu_size;                         // Negotiated MTU for this connection
    layout_json_data_t layout_query_data;      // Layout data buffer
    layout_json_data_t keycode_query_data;     // Keycode data buffer
    layout_json_data_t layout_update_data;     // Layout update buffer
    uint16_t layout_query_offset;              // Layout read offset
    uint16_t keycode_query_offset;             // Keycode read offset
    // Macro-related fields
    char macro_id[64];                         // Current macro ID for queries
    layout_json_data_t macro_query_data;       // Macro data buffer
    layout_json_data_t macro_update_data;      // Macro update buffer
    uint16_t macro_query_offset;               // Macro read offset
} connection_context_t;

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
// Macro-related characteristic handles
static uint16_t macro_id_char_handle = 0;
static uint16_t macro_char_handle = 0;
static uint16_t macro_offset_char_handle = 0;
static uint16_t macro_update_char_handle = 0;

// GATTS profile interface
static esp_gatt_if_t layout_service_gatts_if = 0;

// Connection contexts array
static connection_context_t connection_contexts[MAX_CONCURRENT_CONNECTIONS];

ESP_EVENT_DEFINE_BASE(LAYOUT_EVENTS);

static bool process_layout_update_data(layout_json_data_t *json_data);
static connection_context_t* get_connection_context(uint16_t conn_id);
static connection_context_t* allocate_connection_context(uint16_t conn_id);
static void free_connection_context(uint16_t conn_id);
static void cleanup_connection_data(connection_context_t* ctx);

static esp_err_t layout_post_update_event(uint8_t* json_data, size_t data_len);

// Macro-related helper functions
static esp_err_t process_macro_query_request(connection_context_t* ctx);
static bool process_macro_update_data(layout_json_data_t *json_data);

/**
 * @brief Get connection context for a specific connection ID
 *
 * @param conn_id Connection ID to look up
 * @return Pointer to connection context or NULL if not found
 */
static connection_context_t* get_connection_context(uint16_t conn_id) {
    for (int i = 0; i < MAX_CONCURRENT_CONNECTIONS; i++) {
        if (connection_contexts[i].in_use && connection_contexts[i].conn_id == conn_id) {
            return &connection_contexts[i];
        }
    }
    return NULL;
}

/**
 * @brief Allocate a new connection context for a connection ID
 *
 * @param conn_id Connection ID to allocate context for
 * @return Pointer to allocated context or NULL if no slots available
 */
static connection_context_t* allocate_connection_context(uint16_t conn_id) {
    // First check if we already have a context for this connection
    connection_context_t* existing = get_connection_context(conn_id);
    if (existing) {
        ESP_LOGW(TAG, "Connection context already exists for conn_id %d", conn_id);
        return existing;
    }

    // Find an unused slot
    for (int i = 0; i < MAX_CONCURRENT_CONNECTIONS; i++) {
        if (!connection_contexts[i].in_use) {
            connection_context_t* ctx = &connection_contexts[i];
            memset(ctx, 0, sizeof(connection_context_t));

            ctx->conn_id = conn_id;
            ctx->in_use = true;
            ctx->mtu_size = BLE_DEFAULT_MTU_SIZE;

            // Initialize data buffers to NULL - they'll be allocated when needed
            ctx->layout_query_data.data = NULL;
            ctx->layout_query_data.data_len = 0;
            ctx->layout_query_data.max_len = 0;

            ctx->keycode_query_data.data = NULL;
            ctx->keycode_query_data.data_len = 0;
            ctx->keycode_query_data.max_len = 0;

            ctx->layout_update_data.data = NULL;
            ctx->layout_update_data.data_len = 0;
            ctx->layout_update_data.max_len = 0;

            ctx->layout_query_offset = 0;
            ctx->keycode_query_offset = 0;

            // Initialize macro-related fields
            memset(ctx->macro_id, 0, sizeof(ctx->macro_id));
            ctx->macro_query_data.data = NULL;
            ctx->macro_query_data.data_len = 0;
            ctx->macro_query_data.max_len = 0;

            ctx->macro_update_data.data = NULL;
            ctx->macro_update_data.data_len = 0;
            ctx->macro_update_data.max_len = 0;

            ctx->macro_query_offset = 0;

            ESP_LOGI(TAG, "Allocated connection context for conn_id %d at slot %d", conn_id, i);
            return ctx;
        }
    }

    ESP_LOGE(TAG, "No available connection context slots for conn_id %d", conn_id);
    return NULL;
}

/**
 * @brief Free connection context for a specific connection ID
 *
 * @param conn_id Connection ID to free context for
 */
static void free_connection_context(uint16_t conn_id) {
    connection_context_t* ctx = get_connection_context(conn_id);
    if (ctx) {
        ESP_LOGI(TAG, "Freeing connection context for conn_id %d", conn_id);
        cleanup_connection_data(ctx);
        ctx->in_use = false;
        ctx->conn_id = 0;
    }
}

/**
 * @brief Clean up allocated data for a connection context
 *
 * @param ctx Pointer to connection context to clean up
 */
static void cleanup_connection_data(connection_context_t* ctx) {
    if (!ctx) return;

    if (ctx->layout_query_data.data) {
        free(ctx->layout_query_data.data);
        ctx->layout_query_data.data = NULL;
        ctx->layout_query_data.data_len = 0;
        ctx->layout_query_data.max_len = 0;
    }

    if (ctx->keycode_query_data.data) {
        free(ctx->keycode_query_data.data);
        ctx->keycode_query_data.data = NULL;
        ctx->keycode_query_data.data_len = 0;
        ctx->keycode_query_data.max_len = 0;
    }

    if (ctx->layout_update_data.data) {
        free(ctx->layout_update_data.data);
        ctx->layout_update_data.data = NULL;
        ctx->layout_update_data.data_len = 0;
        ctx->layout_update_data.max_len = 0;
    }

    // Clean up macro-related data
    if (ctx->macro_query_data.data) {
        free(ctx->macro_query_data.data);
        ctx->macro_query_data.data = NULL;
        ctx->macro_query_data.data_len = 0;
        ctx->macro_query_data.max_len = 0;
    }

    if (ctx->macro_update_data.data) {
        free(ctx->macro_update_data.data);
        ctx->macro_update_data.data = NULL;
        ctx->macro_update_data.data_len = 0;
        ctx->macro_update_data.max_len = 0;
    }

    ctx->layout_query_offset = 0;
    ctx->keycode_query_offset = 0;
    ctx->macro_query_offset = 0;
    memset(ctx->macro_id, 0, sizeof(ctx->macro_id));
    ctx->mtu_size = BLE_DEFAULT_MTU_SIZE;
}

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
                                       LAYOUT_UPDATE_POST_TIMEOUT);

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

            esp_err_t ret = esp_ble_gatts_create_service(gatts_if, &service_id, 24); // 24 handles to accommodate all characteristics
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
                layout_service_handle, &layout_uuid, ESP_GATT_PERM_READ_ENCRYPTED,
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
                                      ESP_GATT_PERM_READ_ENCRYPTED | ESP_GATT_PERM_WRITE_ENCRYPTED,
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

                esp_ble_gatts_add_char(layout_service_handle, &keycode_uuid, ESP_GATT_PERM_READ_ENCRYPTED,
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
                                      ESP_GATT_PERM_READ_ENCRYPTED | ESP_GATT_PERM_WRITE_ENCRYPTED,
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
                                      ESP_GATT_PERM_WRITE_ENCRYPTED,  // Write-only permission
                                      ESP_GATT_CHAR_PROP_BIT_WRITE,
                                      NULL, NULL);
            }
            else if (param->add_char.char_uuid.uuid.uuid16 == LAYOUT_UPDATE_CHAR_UUID)
            {
                layout_update_char_handle = param->add_char.attr_handle;
                ESP_LOGI(TAG, "Layout update characteristic added, handle: %d", layout_update_char_handle);

                // Add macro ID characteristic (write-only)
                esp_bt_uuid_t macro_id_uuid;
                macro_id_uuid.len = ESP_UUID_LEN_16;
                macro_id_uuid.uuid.uuid16 = MACRO_ID_CHAR_UUID;

                esp_ble_gatts_add_char(layout_service_handle, &macro_id_uuid,
                                      ESP_GATT_PERM_WRITE_ENCRYPTED,
                                      ESP_GATT_CHAR_PROP_BIT_WRITE,
                                      NULL, NULL);
            }
            else if (param->add_char.char_uuid.uuid.uuid16 == MACRO_ID_CHAR_UUID)
            {
                macro_id_char_handle = param->add_char.attr_handle;
                ESP_LOGI(TAG, "Macro ID characteristic added, handle: %d", macro_id_char_handle);

                // Add macro characteristic (read-only)
                esp_bt_uuid_t macro_uuid;
                macro_uuid.len = ESP_UUID_LEN_16;
                macro_uuid.uuid.uuid16 = MACRO_CHAR_UUID;

                esp_ble_gatts_add_char(layout_service_handle, &macro_uuid,
                                      ESP_GATT_PERM_READ_ENCRYPTED,
                                      ESP_GATT_CHAR_PROP_BIT_READ,
                                      NULL, NULL);
            }
            else if (param->add_char.char_uuid.uuid.uuid16 == MACRO_CHAR_UUID)
            {
                macro_char_handle = param->add_char.attr_handle;
                ESP_LOGI(TAG, "Macro characteristic added, handle: %d", macro_char_handle);

                // Add macro offset characteristic (write-only)
                esp_bt_uuid_t macro_offset_uuid;
                macro_offset_uuid.len = ESP_UUID_LEN_16;
                macro_offset_uuid.uuid.uuid16 = MACRO_OFFSET_CHAR_UUID;

                esp_ble_gatts_add_char(layout_service_handle, &macro_offset_uuid,
                                      ESP_GATT_PERM_WRITE_ENCRYPTED,
                                      ESP_GATT_CHAR_PROP_BIT_WRITE,
                                      NULL, NULL);
            }
            else if (param->add_char.char_uuid.uuid.uuid16 == MACRO_OFFSET_CHAR_UUID)
            {
                macro_offset_char_handle = param->add_char.attr_handle;
                ESP_LOGI(TAG, "Macro offset characteristic added, handle: %d", macro_offset_char_handle);

                // Add macro update characteristic (write-only)
                esp_bt_uuid_t macro_update_uuid;
                macro_update_uuid.len = ESP_UUID_LEN_16;
                macro_update_uuid.uuid.uuid16 = MACRO_UPDATE_CHAR_UUID;

                esp_ble_gatts_add_char(layout_service_handle, &macro_update_uuid,
                                      ESP_GATT_PERM_WRITE_ENCRYPTED,
                                      ESP_GATT_CHAR_PROP_BIT_WRITE,
                                      NULL, NULL);
            }
            else if (param->add_char.char_uuid.uuid.uuid16 == MACRO_UPDATE_CHAR_UUID)
            {
                macro_update_char_handle = param->add_char.attr_handle;
                ESP_LOGI(TAG, "Macro update characteristic added, handle: %d", macro_update_char_handle);
            }
        }
        else
        {
            ESP_LOGE(TAG, "Add characteristic failed, status %d", param->add_char.status);
        }
        break;

    case ESP_GATTS_CONNECT_EVT:
        ESP_LOGI(TAG, "Client connected, conn_id: %d", param->connect.conn_id);
        // Allocate connection context for this new connection
        if (allocate_connection_context(param->connect.conn_id) == NULL) {
            ESP_LOGE(TAG, "Failed to allocate connection context for conn_id %d", param->connect.conn_id);
        }
        break;

    case ESP_GATTS_DISCONNECT_EVT:
        ESP_LOGI(TAG, "Client disconnected, conn_id: %d", param->disconnect.conn_id);
        // Free connection context for this disconnected connection
        free_connection_context(param->disconnect.conn_id);
        break;

    case ESP_GATTS_MTU_EVT:
        ESP_LOGI(TAG, "MTU negotiated: %d for conn_id: %d", param->mtu.mtu, param->mtu.conn_id);
        // Store the negotiated MTU for the specific connection
        connection_context_t* mtu_ctx = get_connection_context(param->mtu.conn_id);
        if (mtu_ctx) {
            mtu_ctx->mtu_size = param->mtu.mtu;
        } else {
            ESP_LOGW(TAG, "No connection context found for MTU event, conn_id: %d", param->mtu.conn_id);
        }
        break;

    case ESP_GATTS_READ_EVT:
        if (param->read.handle == layout_char_handle)
        {
            // Get connection context for this specific connection
            connection_context_t* ctx = get_connection_context(param->read.conn_id);
            if (!ctx) {
                ESP_LOGE(TAG, "No connection context found for conn_id %d", param->read.conn_id);
                esp_ble_gatts_send_response(gatts_if, param->read.conn_id, param->read.trans_id, ESP_GATT_INTERNAL_ERROR, NULL);
                break;
            }

            esp_gatt_rsp_t rsp = {0};
            if (param->read.offset != 0) {
                rsp.attr_value.len = 0;
                rsp.attr_value.handle = param->read.handle;
                rsp.attr_value.offset = param->read.offset;
                esp_ble_gatts_send_response(gatts_if, param->read.conn_id, param->read.trans_id, ESP_GATT_OK, &rsp);
                break;
            }

            if (ctx->layout_query_data.data == NULL)
            {
                // Initialize the layout query data buffer if not already done
                ctx->layout_query_data.max_len = LAYOUT_JSON_MAX_SIZE;
                ctx->layout_query_data.data = malloc(ctx->layout_query_data.max_len);
                ctx->layout_query_data.data_len = 0;
                if (!ctx->layout_query_data.data)
                {
                    ESP_LOGE(TAG, "Failed to allocate memory for layout query data");
                    esp_ble_gatts_send_response(gatts_if, param->read.conn_id, param->read.trans_id, ESP_GATT_INSUF_RESOURCE, NULL);
                    break;
                }
            }

            // Generate the JSON using our utility function
            generate_layouts_json(ble_append_str, &ctx->layout_query_data);

            // Use the negotiated MTU for this connection
            uint16_t mtu = ctx->mtu_size;

            // Use the offset from the connection context
            uint16_t effective_offset = ctx->layout_query_offset;
            uint16_t available_len = 0;

            if (effective_offset < ctx->layout_query_data.data_len) {
                available_len = ctx->layout_query_data.data_len - effective_offset;
            }

            uint16_t send_len = MIN(available_len, MIN(BLE_MAX_ATTR_LEN, mtu - 1)); // MTU minus 1-byte opcode

            if (effective_offset >= ctx->layout_query_data.data_len)
            {
                // Offset is beyond the data length, send empty response
                send_len = 0;
            }

            ESP_LOGI(TAG, "Layout read request, conn_id: %d, stored offset: %d, request offset: %d, effective offset: %d, available: %d, sending: %d",
                     param->read.conn_id, ctx->layout_query_offset, param->read.offset, effective_offset, available_len, send_len);

            rsp.attr_value.len = send_len;
            rsp.attr_value.handle = param->read.handle;
            rsp.attr_value.offset = 0;

            if (send_len > 0)
            {
                memcpy(rsp.attr_value.value, ctx->layout_query_data.data + effective_offset, send_len);
            }

            // Send response
            esp_err_t ret =
                esp_ble_gatts_send_response(gatts_if, param->read.conn_id, param->read.trans_id, ESP_GATT_OK, &rsp);
            if (ret != ESP_OK)
            {
                ESP_LOGE(TAG, "Send response failed: %d", ret);
            }

            free(ctx->layout_query_data.data);
            ctx->layout_query_data.data = NULL; // Free the buffer after sending
            ctx->layout_query_data.data_len = 0; // Reset the length
        }
        else if (param->read.handle == keycode_char_handle)
        {
            // Get connection context for this specific connection
            connection_context_t* ctx = get_connection_context(param->read.conn_id);
            if (!ctx) {
                ESP_LOGE(TAG, "No connection context found for conn_id %d", param->read.conn_id);
                esp_ble_gatts_send_response(gatts_if, param->read.conn_id, param->read.trans_id, ESP_GATT_INTERNAL_ERROR, NULL);
                break;
            }

            esp_gatt_rsp_t rsp = {0};
            if (param->read.offset != 0) {
                rsp.attr_value.len = 0;
                rsp.attr_value.handle = param->read.handle;
                rsp.attr_value.offset = param->read.offset;
                esp_ble_gatts_send_response(gatts_if, param->read.conn_id, param->read.trans_id, ESP_GATT_OK, &rsp);
                break;
            }

            if (ctx->keycode_query_data.data == NULL)
            {
                // Initialize the keycode query data buffer if not already done
                ctx->keycode_query_data.max_len = LAYOUT_JSON_MAX_SIZE;
                ctx->keycode_query_data.data = malloc(ctx->keycode_query_data.max_len);
                ctx->keycode_query_data.data_len = 0;
                if (!ctx->keycode_query_data.data)
                {
                    ESP_LOGE(TAG, "Failed to allocate memory for keycode query data");
                    esp_ble_gatts_send_response(gatts_if, param->read.conn_id, param->read.trans_id, ESP_GATT_INSUF_RESOURCE, NULL);
                    break;
                }
            }

            // Generate the JSON using our utility function
            generate_keycodes_json(ble_append_str, &ctx->keycode_query_data);

            // Use the negotiated MTU for this connection
            uint16_t mtu = ctx->mtu_size;

            // Use the offset from the connection context
            uint16_t effective_offset = ctx->keycode_query_offset;
            uint16_t available_len = 0;

            if (effective_offset < ctx->keycode_query_data.data_len) {
                available_len = ctx->keycode_query_data.data_len - effective_offset;
            }

            uint16_t send_len = MIN(available_len, MIN(BLE_MAX_ATTR_LEN, mtu - 1)); // MTU minus 1-byte opcode

            if (effective_offset >= ctx->keycode_query_data.data_len)
            {
                // Offset is beyond the data length, send empty response
                send_len = 0;
            }

            ESP_LOGI(TAG, "Keycode read request, conn_id: %d, stored offset: %d, request offset: %d, effective offset: %d, available: %d, sending: %d",
                     param->read.conn_id, ctx->keycode_query_offset, param->read.offset, effective_offset, available_len, send_len);

            rsp.attr_value.len = send_len;
            rsp.attr_value.handle = param->read.handle;
            rsp.attr_value.offset = 0;

            if (send_len > 0)
            {
                memcpy(rsp.attr_value.value, ctx->keycode_query_data.data + effective_offset, send_len);
            }

            // Send response
            esp_err_t ret =
                esp_ble_gatts_send_response(gatts_if, param->read.conn_id, param->read.trans_id, ESP_GATT_OK, &rsp);
            if (ret != ESP_OK)
            {
                ESP_LOGE(TAG, "Send response failed: %d", ret);
            }

            free(ctx->keycode_query_data.data);
            ctx->keycode_query_data.data_len = 0; // Reset the length
            ctx->keycode_query_data.data = NULL; // Free the buffer after sending
        }
        else if (param->read.handle == layout_offset_char_handle)
        {
            // Get connection context for this specific connection
            connection_context_t* ctx = get_connection_context(param->read.conn_id);
            if (!ctx) {
                ESP_LOGE(TAG, "No connection context found for conn_id %d", param->read.conn_id);
                esp_ble_gatts_send_response(gatts_if, param->read.conn_id, param->read.trans_id, ESP_GATT_INTERNAL_ERROR, NULL);
                break;
            }

            // Handle layout offset read request
            esp_gatt_rsp_t rsp = {0};

            rsp.attr_value.handle = param->read.handle;
            rsp.attr_value.len = 2; // 16-bit value (2 bytes)
            rsp.attr_value.offset = 0;

            // Set the value (little-endian)
            rsp.attr_value.value[0] = ctx->layout_query_offset & 0xFF;
            rsp.attr_value.value[1] = (ctx->layout_query_offset >> 8) & 0xFF;

            ESP_LOGI(TAG, "Layout offset read: %d for conn_id: %d", ctx->layout_query_offset, param->read.conn_id);

            // Send response
            esp_ble_gatts_send_response(gatts_if, param->read.conn_id, param->read.trans_id, ESP_GATT_OK, &rsp);
        }
        else if (param->read.handle == keycode_offset_char_handle)
        {
            // Get connection context for this specific connection
            connection_context_t* ctx = get_connection_context(param->read.conn_id);
            if (!ctx) {
                ESP_LOGE(TAG, "No connection context found for conn_id %d", param->read.conn_id);
                esp_ble_gatts_send_response(gatts_if, param->read.conn_id, param->read.trans_id, ESP_GATT_INTERNAL_ERROR, NULL);
                break;
            }

            // Handle keycode offset read request
            esp_gatt_rsp_t rsp = {0};

            rsp.attr_value.handle = param->read.handle;
            rsp.attr_value.len = 2; // 16-bit value (2 bytes)
            rsp.attr_value.offset = 0;

            // Set the value (little-endian)
            rsp.attr_value.value[0] = ctx->keycode_query_offset & 0xFF;
            rsp.attr_value.value[1] = (ctx->keycode_query_offset >> 8) & 0xFF;

            ESP_LOGI(TAG, "Keycode offset read: %d for conn_id: %d", ctx->keycode_query_offset, param->read.conn_id);

            // Send response
            esp_ble_gatts_send_response(gatts_if, param->read.conn_id, param->read.trans_id, ESP_GATT_OK, &rsp);
        }
        else if (param->read.handle == macro_char_handle)
        {
            // Get connection context for this specific connection
            connection_context_t* ctx = get_connection_context(param->read.conn_id);
            if (!ctx) {
                ESP_LOGE(TAG, "No connection context found for conn_id %d", param->read.conn_id);
                esp_ble_gatts_send_response(gatts_if, param->read.conn_id, param->read.trans_id, ESP_GATT_INTERNAL_ERROR, NULL);
                break;
            }

            esp_gatt_rsp_t rsp = {0};
            if (param->read.offset != 0) {
                rsp.attr_value.len = 0;
                rsp.attr_value.handle = param->read.handle;
                rsp.attr_value.offset = param->read.offset;
                esp_ble_gatts_send_response(gatts_if, param->read.conn_id, param->read.trans_id, ESP_GATT_OK, &rsp);
                break;
            }

            // Process macro query request
            esp_err_t ret = process_macro_query_request(ctx);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "Failed to process macro query request");
                esp_ble_gatts_send_response(gatts_if, param->read.conn_id, param->read.trans_id, ESP_GATT_INTERNAL_ERROR, NULL);
                break;
            }

            // Use the negotiated MTU for this connection
            uint16_t mtu = ctx->mtu_size;

            // Use the offset from the connection context
            uint16_t effective_offset = ctx->macro_query_offset;
            uint16_t available_len = 0;

            if (effective_offset < ctx->macro_query_data.data_len) {
                available_len = ctx->macro_query_data.data_len - effective_offset;
            }

            uint16_t send_len = MIN(available_len, MIN(BLE_MAX_ATTR_LEN, mtu - 1)); // MTU minus 1-byte opcode

            if (effective_offset >= ctx->macro_query_data.data_len)
            {
                // Offset is beyond the data length, send empty response
                send_len = 0;
            }

            ESP_LOGI(TAG, "Macro read request, conn_id: %d, stored offset: %d, request offset: %d, effective offset: %d, available: %d, sending: %d",
                     param->read.conn_id, ctx->macro_query_offset, param->read.offset, effective_offset, available_len, send_len);

            rsp.attr_value.len = send_len;
            rsp.attr_value.handle = param->read.handle;
            rsp.attr_value.offset = 0;

            if (send_len > 0)
            {
                memcpy(rsp.attr_value.value, ctx->macro_query_data.data + effective_offset, send_len);
            }

            // Send response
            esp_err_t resp_ret = esp_ble_gatts_send_response(gatts_if, param->read.conn_id, param->read.trans_id, ESP_GATT_OK, &rsp);
            if (resp_ret != ESP_OK)
            {
                ESP_LOGE(TAG, "Send response failed: %d", resp_ret);
            }

            free(ctx->macro_query_data.data);
            ctx->macro_query_data.data_len = 0; // Reset the length
            ctx->macro_query_data.data = NULL; // Free the buffer after sending
        }
        break;

    case ESP_GATTS_WRITE_EVT:
        if (!param->write.is_prep)
        {
            // This is a complete write operation (not a prepare write)
            if (param->write.handle == layout_offset_char_handle)
            {
                // Get connection context for this specific connection
                connection_context_t* ctx = get_connection_context(param->write.conn_id);
                if (!ctx) {
                    ESP_LOGE(TAG, "No connection context found for conn_id %d", param->write.conn_id);
                    if (param->write.need_rsp) {
                        esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, ESP_GATT_INTERNAL_ERROR, NULL);
                    }
                    break;
                }

                // Process layout offset write
                if (param->write.len >= 2)
                {
                    // Extract 16-bit offset value (little-endian)
                    uint16_t new_offset = param->write.value[0] | (param->write.value[1] << 8);

                    // Always allow setting the offset - even if it's beyond the data length
                    ctx->layout_query_offset = new_offset;
                    ESP_LOGI(TAG, "Layout offset set to: %d for conn_id: %d (data length: %d)",
                             ctx->layout_query_offset, param->write.conn_id,
                             ctx->layout_query_data.data ? ctx->layout_query_data.data_len : 0);
                }
            }
            else if (param->write.handle == keycode_offset_char_handle)
            {
                // Get connection context for this specific connection
                connection_context_t* ctx = get_connection_context(param->write.conn_id);
                if (!ctx) {
                    ESP_LOGE(TAG, "No connection context found for conn_id %d", param->write.conn_id);
                    if (param->write.need_rsp) {
                        esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, ESP_GATT_INTERNAL_ERROR, NULL);
                    }
                    break;
                }

                // Process keycode offset write
                if (param->write.len >= 2)
                {
                    // Extract 16-bit offset value (little-endian)
                    uint16_t new_offset = param->write.value[0] | (param->write.value[1] << 8);

                    // Always allow setting the offset - even if it's beyond the data length
                    ctx->keycode_query_offset = new_offset;
                    ESP_LOGI(TAG, "Keycode offset set to: %d for conn_id: %d (data length: %d)",
                             ctx->keycode_query_offset, param->write.conn_id,
                             ctx->keycode_query_data.data ? ctx->keycode_query_data.data_len : 0);
                }
            }
            else if (param->write.handle == layout_update_char_handle)
            {
                // Get connection context for this specific connection
                connection_context_t* ctx = get_connection_context(param->write.conn_id);
                if (!ctx) {
                    ESP_LOGE(TAG, "No connection context found for conn_id %d", param->write.conn_id);
                    if (param->write.need_rsp) {
                        esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, ESP_GATT_INTERNAL_ERROR, NULL);
                    }
                    break;
                }

                // Process layout update write (JSON data from client)
                ESP_LOGI(TAG, "Received layout update data, length: %d for conn_id: %d", param->write.len, param->write.conn_id);

                if (ctx->layout_update_data.data == NULL)
                {
                    // Allocate initial buffer if not already done
                    ctx->layout_update_data.max_len = LAYOUT_JSON_MAX_SIZE;
                    ctx->layout_update_data.data = malloc(ctx->layout_update_data.max_len);
                    ctx->layout_update_data.data_len = 0;
                    if (ctx->layout_update_data.data == NULL)
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
                if (ctx->layout_update_data.data_len + param->write.len <= ctx->layout_update_data.max_len)
                {
                    // Append the new data to our buffer
                    memcpy(ctx->layout_update_data.data + ctx->layout_update_data.data_len,
                           param->write.value,
                           param->write.len);
                    ctx->layout_update_data.data_len += param->write.len;

                    // Ensure null-termination for JSON processing
                    ctx->layout_update_data.data[ctx->layout_update_data.data_len] = '\0';

                    ESP_LOGI(TAG, "Updated layout data buffer, total length: %d for conn_id: %d",
                             ctx->layout_update_data.data_len, param->write.conn_id);

                    if (ctx->layout_update_data.data_len > 0) {

                        // Process the complete layout update data
                        process_layout_update_data(&ctx->layout_update_data);
                    }
                }
                else
                {
                    ESP_LOGE(TAG, "Layout update buffer overflow, discarding update for conn_id: %d", param->write.conn_id);
                    // Reset the buffer for next update
                    ctx->layout_update_data.data_len = 0;
                    if (param->write.need_rsp)
                    {
                        esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id,
                                                    ESP_GATT_INVALID_ATTR_LEN, NULL);
                        break; // Skip the standard response below
                    }
                }
            }
            else if (param->write.handle == macro_id_char_handle)
            {
                // Get connection context for this specific connection
                connection_context_t* ctx = get_connection_context(param->write.conn_id);
                if (!ctx) {
                    ESP_LOGE(TAG, "No connection context found for conn_id %d", param->write.conn_id);
                    if (param->write.need_rsp) {
                        esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, ESP_GATT_INTERNAL_ERROR, NULL);
                    }
                    break;
                }

                // Process macro ID write (null-terminated string)
                if (param->write.len > 0 && param->write.len < sizeof(ctx->macro_id))
                {
                    memcpy(ctx->macro_id, param->write.value, param->write.len);
                    ctx->macro_id[param->write.len] = '\0'; // Null-terminate
                    ESP_LOGI(TAG, "Macro ID set to: '%s' for conn_id: %d", ctx->macro_id, param->write.conn_id);
                }
                else
                {
                    ESP_LOGE(TAG, "Invalid macro ID length: %d for conn_id: %d", param->write.len, param->write.conn_id);
                }
            }
            else if (param->write.handle == macro_offset_char_handle)
            {
                // Get connection context for this specific connection
                connection_context_t* ctx = get_connection_context(param->write.conn_id);
                if (!ctx) {
                    ESP_LOGE(TAG, "No connection context found for conn_id %d", param->write.conn_id);
                    if (param->write.need_rsp) {
                        esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, ESP_GATT_INTERNAL_ERROR, NULL);
                    }
                    break;
                }

                // Process macro offset write
                if (param->write.len >= 2)
                {
                    // Extract 16-bit offset value (little-endian)
                    uint16_t new_offset = param->write.value[0] | (param->write.value[1] << 8);

                    // Always allow setting the offset - even if it's beyond the data length
                    ctx->macro_query_offset = new_offset;
                    ESP_LOGI(TAG, "Macro offset set to: %d for conn_id: %d (data length: %d)",
                             ctx->macro_query_offset, param->write.conn_id,
                             ctx->macro_query_data.data ? ctx->macro_query_data.data_len : 0);
                }
            }
            else if (param->write.handle == macro_update_char_handle)
            {
                // Get connection context for this specific connection
                connection_context_t* ctx = get_connection_context(param->write.conn_id);
                if (!ctx) {
                    ESP_LOGE(TAG, "No connection context found for conn_id %d", param->write.conn_id);
                    if (param->write.need_rsp) {
                        esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, ESP_GATT_INTERNAL_ERROR, NULL);
                    }
                    break;
                }

                // Process macro update write (JSON data from client)
                ESP_LOGI(TAG, "Received macro update data, length: %d for conn_id: %d", param->write.len, param->write.conn_id);

                if (ctx->macro_update_data.data == NULL)
                {
                    // Allocate initial buffer if not already done
                    ctx->macro_update_data.max_len = LAYOUT_JSON_MAX_SIZE;
                    ctx->macro_update_data.data = malloc(ctx->macro_update_data.max_len);
                    ctx->macro_update_data.data_len = 0;
                    if (ctx->macro_update_data.data == NULL)
                    {
                        ESP_LOGE(TAG, "Failed to allocate memory for macro update data");
                        if (param->write.need_rsp)
                        {
                            esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id,
                                                        ESP_GATT_INSUF_RESOURCE, NULL);
                            break; // Skip the standard response below
                        }
                    }
                }

                // Check if we have enough space in our buffer
                if (ctx->macro_update_data.data_len + param->write.len <= ctx->macro_update_data.max_len)
                {
                    // Append the new data to our buffer
                    memcpy(ctx->macro_update_data.data + ctx->macro_update_data.data_len,
                           param->write.value,
                           param->write.len);
                    ctx->macro_update_data.data_len += param->write.len;

                    // Ensure null-termination for JSON processing
                    ctx->macro_update_data.data[ctx->macro_update_data.data_len] = '\0';

                    ESP_LOGI(TAG, "Updated macro data buffer, total length: %d for conn_id: %d",
                             ctx->macro_update_data.data_len, param->write.conn_id);

                    if (ctx->macro_update_data.data_len > 0) {
                        // Process the complete macro update data
                        process_macro_update_data(&ctx->macro_update_data);
                    }
                }
                else
                {
                    ESP_LOGE(TAG, "Macro update buffer overflow, discarding update for conn_id: %d", param->write.conn_id);
                    // Reset the buffer for next update
                    ctx->macro_update_data.data_len = 0;
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
            else if (param->write.handle == layout_char_handle || param->write.handle == keycode_char_handle || param->write.handle == macro_char_handle)
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
                // Get connection context for this specific connection
                connection_context_t* ctx = get_connection_context(param->write.conn_id);
                if (!ctx) {
                    ESP_LOGE(TAG, "No connection context found for conn_id %d", param->write.conn_id);
                    esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, ESP_GATT_INTERNAL_ERROR, NULL);
                    break;
                }

                ESP_LOGI(TAG, "Prepare write for layout update, offset: %d, length: %d for conn_id: %d",
                         param->write.offset, param->write.len, param->write.conn_id);

                if (ctx->layout_update_data.data == NULL)
                {
                    // Allocate initial buffer if not already done
                    ctx->layout_update_data.max_len = LAYOUT_JSON_MAX_SIZE;
                    ctx->layout_update_data.data = malloc(ctx->layout_update_data.max_len);
                    ctx->layout_update_data.data_len = 0;
                    if (ctx->layout_update_data.data == NULL)
                    {
                        ESP_LOGE(TAG, "Failed to allocate memory for layout update data");
                        esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id,
                                                    ESP_GATT_INSUF_RESOURCE, NULL);
                        break; // Skip the standard response below
                    }
                }

                // Check if the offset is valid and we have enough buffer space
                if (param->write.offset + param->write.len <= ctx->layout_update_data.max_len)
                {
                    // For prepare writes, we store data at the specified offset
                    // This allows the client to send data out of order if needed
                    if (param->write.offset + param->write.len > ctx->layout_update_data.data_len)
                    {
                        ctx->layout_update_data.data_len = param->write.offset + param->write.len;
                    }

                    // Copy data to the buffer at the specified offset
                    memcpy(ctx->layout_update_data.data + param->write.offset, param->write.value, param->write.len);

                    // Prepare writes always need a response (as per BLE specification)
                    esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, ESP_GATT_OK, NULL);
                }
                else
                {
                    ESP_LOGE(TAG, "Prepare write buffer overflow, offset: %d, len: %d, max: %d for conn_id: %d",
                             param->write.offset, param->write.len, ctx->layout_update_data.max_len, param->write.conn_id);
                    // Prepare writes always need a response with appropriate error code
                    esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id,
                                               ESP_GATT_PREPARE_Q_FULL, NULL);
                }
            }
            else if (param->write.handle == macro_update_char_handle)
            {
                // Get connection context for this specific connection
                connection_context_t* ctx = get_connection_context(param->write.conn_id);
                if (!ctx) {
                    ESP_LOGE(TAG, "No connection context found for conn_id %d", param->write.conn_id);
                    esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, ESP_GATT_INTERNAL_ERROR, NULL);
                    break;
                }

                ESP_LOGI(TAG, "Prepare write for macro update, offset: %d, length: %d for conn_id: %d",
                         param->write.offset, param->write.len, param->write.conn_id);

                if (ctx->macro_update_data.data == NULL)
                {
                    // Allocate initial buffer if not already done
                    ctx->macro_update_data.max_len = LAYOUT_JSON_MAX_SIZE;
                    ctx->macro_update_data.data = malloc(ctx->macro_update_data.max_len);
                    ctx->macro_update_data.data_len = 0;
                    if (ctx->macro_update_data.data == NULL)
                    {
                        ESP_LOGE(TAG, "Failed to allocate memory for macro update data");
                        esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id,
                                                    ESP_GATT_INSUF_RESOURCE, NULL);
                        break; // Skip the standard response below
                    }
                }

                // Check if the offset is valid and we have enough buffer space
                if (param->write.offset + param->write.len <= ctx->macro_update_data.max_len)
                {
                    // For prepare writes, we store data at the specified offset
                    // This allows the client to send data out of order if needed
                    if (param->write.offset + param->write.len > ctx->macro_update_data.data_len)
                    {
                        ctx->macro_update_data.data_len = param->write.offset + param->write.len;
                    }

                    // Copy data to the buffer at the specified offset
                    memcpy(ctx->macro_update_data.data + param->write.offset, param->write.value, param->write.len);

                    // Prepare writes always need a response (as per BLE specification)
                    esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, ESP_GATT_OK, NULL);
                }
                else
                {
                    ESP_LOGE(TAG, "Prepare write buffer overflow, offset: %d, len: %d, max: %d for conn_id: %d",
                             param->write.offset, param->write.len, ctx->macro_update_data.max_len, param->write.conn_id);
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
            ESP_LOGI(TAG, "Executing prepared writes for conn_id: %d", param->exec_write.conn_id);

            // Get connection context for this specific connection
            connection_context_t* ctx = get_connection_context(param->exec_write.conn_id);
            if (ctx) {
                // Process layout update data if available
                if (ctx->layout_update_data.data_len > 0)
                {
                    // Ensure null-termination for JSON processing
                    ctx->layout_update_data.data[ctx->layout_update_data.data_len] = '\0';

                    ESP_LOGI(TAG, "Received complete layout update JSON data, length: %d for conn_id: %d",
                             ctx->layout_update_data.data_len, param->exec_write.conn_id);

                    // Process the data using our shared helper function
                    process_layout_update_data(&ctx->layout_update_data);
                }

                // Process macro update data if available
                if (ctx->macro_update_data.data_len > 0)
                {
                    // Ensure null-termination for JSON processing
                    ctx->macro_update_data.data[ctx->macro_update_data.data_len] = '\0';

                    ESP_LOGI(TAG, "Received complete macro update JSON data, length: %d for conn_id: %d",
                             ctx->macro_update_data.data_len, param->exec_write.conn_id);

                    // Process the data using our shared helper function
                    process_macro_update_data(&ctx->macro_update_data);
                }

                if (ctx->layout_update_data.data_len == 0 && ctx->macro_update_data.data_len == 0) {
                    ESP_LOGW(TAG, "No update data to execute for conn_id: %d", param->exec_write.conn_id);
                }
            }
            else
            {
                ESP_LOGW(TAG, "No connection context found for execute write, conn_id: %d", param->exec_write.conn_id);
            }
        }
        else
        {
            // Cancel prepared writes
            ESP_LOGI(TAG, "Cancelling prepared writes for conn_id: %d", param->exec_write.conn_id);

            // Get connection context and reset buffers if writes are cancelled
            connection_context_t* ctx = get_connection_context(param->exec_write.conn_id);
            if (ctx) {
                ctx->layout_update_data.data_len = 0;
                ctx->macro_update_data.data_len = 0;
            }
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
    // Initialize connection contexts array
    for (int i = 0; i < MAX_CONCURRENT_CONNECTIONS; i++) {
        connection_contexts[i].in_use = false;
        connection_contexts[i].conn_id = 0;
        connection_contexts[i].mtu_size = BLE_DEFAULT_MTU_SIZE;

        connection_contexts[i].layout_query_data.data = NULL;
        connection_contexts[i].layout_query_data.data_len = 0;
        connection_contexts[i].layout_query_data.max_len = 0;

        connection_contexts[i].keycode_query_data.data = NULL;
        connection_contexts[i].keycode_query_data.data_len = 0;
        connection_contexts[i].keycode_query_data.max_len = 0;

        connection_contexts[i].layout_update_data.data = NULL;
        connection_contexts[i].layout_update_data.data_len = 0;
        connection_contexts[i].layout_update_data.max_len = 0;

        connection_contexts[i].layout_query_offset = 0;
        connection_contexts[i].keycode_query_offset = 0;

        // Initialize macro-related fields
        memset(connection_contexts[i].macro_id, 0, sizeof(connection_contexts[i].macro_id));
        connection_contexts[i].macro_query_data.data = NULL;
        connection_contexts[i].macro_query_data.data_len = 0;
        connection_contexts[i].macro_query_data.max_len = 0;

        connection_contexts[i].macro_update_data.data = NULL;
        connection_contexts[i].macro_update_data.data_len = 0;
        connection_contexts[i].macro_update_data.max_len = 0;

        connection_contexts[i].macro_query_offset = 0;
    }

    ESP_LOGI(TAG, "Initialized %d connection context slots", MAX_CONCURRENT_CONNECTIONS);

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

/**
 * @brief Process macro query request and populate macro data buffer
 *
 * @param ctx Connection context (macro_id field is ignored - returns all macros)
 * @return ESP_OK on success, error code otherwise
 */
static esp_err_t process_macro_query_request(connection_context_t* ctx)
{
    if (!ctx) {
        ESP_LOGE(TAG, "Invalid macro query request - no context provided");
        return ESP_ERR_INVALID_ARG;
    }

    // Initialize the macro query data buffer if not already done
    if (ctx->macro_query_data.data == NULL) {
        ctx->macro_query_data.max_len = LAYOUT_JSON_MAX_SIZE;
        ctx->macro_query_data.data = malloc(ctx->macro_query_data.max_len);
        ctx->macro_query_data.data_len = 0;
        if (!ctx->macro_query_data.data) {
            ESP_LOGE(TAG, "Failed to allocate memory for macro query data");
            return ESP_ERR_NO_MEM;
        }
    }

    // Reset buffer for new data
    ctx->macro_query_data.data_len = 0;
    ctx->macro_query_data.data[0] = '\0';

    // Use the shared utility function to generate all macros JSON
    esp_err_t ret = generate_macros_json(ble_append_str, &ctx->macro_query_data);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to generate macros JSON");
        return ret;
    }

    ESP_LOGI(TAG, "Generated all macros JSON, length: %d", ctx->macro_query_data.data_len);
    return ESP_OK;
}

/**
 * @brief Process macro update data and apply changes
 *
 * @param json_data Pointer to the JSON data buffer containing macro update
 * @return true if successful, false otherwise
 */
static bool process_macro_update_data(layout_json_data_t *json_data)
{
    if (json_data->data == NULL || json_data->data_len == 0) {
        ESP_LOGE(TAG, "Invalid macro update data");
        return false;
    }

    ESP_LOGI(TAG, "Processing macro update data, length: %d", json_data->data_len);

    // Parse JSON data
    cJSON* root = cJSON_Parse((char*)json_data->data);
    if (!root) {
        ESP_LOGE(TAG, "Failed to parse macro update JSON");
        return false;
    }

    // Use the shared utility function to update macros
    esp_err_t ret = update_macros_from_json(root);
    bool success = (ret == ESP_OK);

    cJSON_Delete(root);

    // Reset the data after processing
    json_data->data_len = 0;
    ESP_LOGI(TAG, "Macro update processing completed, success: %s", success ? "true" : "false");
    return success;
}