/*
 *  This program is a keyboard firmware for ESP family boards inspired by
 *  MK32 and qmk_firmware.
 *
 *  Copyright (C) 2024 github.com/paul356
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_random.h"
#include "esp_check.h"
#include "keyboard_config.h"
#include "key_definitions.h"
#include "layout_store.h"
#include "cJSON.h"
#include "hal_ble.h"
#include "function_control.h"
#include "macros.h"
#include "function_key.h"
#include "keymap_json.h"

#define TAG "[HTTPD]"
#define ARRAY_LEN(arr) (sizeof(arr)/sizeof(arr[0]))
#define RECV_BUF_SIZE 2048

static time_t s_init_version;
static char recv_buf[RECV_BUF_SIZE];

static esp_err_t parse_http_req(httpd_req_t* req, cJSON** root)
{
    if (!req || !root) {
        return ESP_ERR_INVALID_ARG;
    }

    // Check for reasonable content length to prevent memory exhaustion
    size_t buf_len = req->content_len;
    if (buf_len == 0) {
        ESP_LOGW(TAG, "Empty request body");
        return ESP_ERR_INVALID_ARG;
    }

    // Allocate memory for the request body
    char* body = (char*)malloc(buf_len + 1); // +1 for null terminator
    if (!body) {
        ESP_LOGE(TAG, "Failed to allocate memory for request body");
        return ESP_ERR_NO_MEM;
    }

    // Read the request body with timeout handling
    int received = 0;
    int total_received = 0;
    int remaining = buf_len;

    while (remaining > 0) {
        received = httpd_req_recv(req, body + total_received, remaining);
        if (received <= 0) {
            if (received == HTTPD_SOCK_ERR_TIMEOUT) {
                // Timeout, try again
                continue;
            }
            // Other error
            ESP_LOGE(TAG, "Error receiving request data: %d", received);
            free(body);
            return ESP_ERR_INVALID_STATE;
        }

        total_received += received;
        remaining -= received;
    }

    // Ensure null termination for safer JSON parsing
    body[total_received] = '\0';

    // Parse the JSON
    *root = cJSON_Parse(body);
    free(body);

    // Check for JSON parsing errors
    if (*root == NULL) {
        const char* error_ptr = cJSON_GetErrorPtr();
        if (error_ptr != NULL) {
            ESP_LOGE(TAG, "JSON parsing error before: %s", error_ptr);
        }
        // Maybe invalid json
        return ESP_ERR_INVALID_ARG;
    }

    return ESP_OK;
}

static esp_err_t serve_static_files(httpd_req_t* req)
{
    extern const unsigned char index_html_start[] asm("_binary_index_html_start");
    extern const unsigned char index_html_end[] asm("_binary_index_html_end");

    extern const unsigned char app_js_start[] asm("_binary_app_js_start");
    extern const unsigned char app_js_end[] asm("_binary_app_js_end");

    extern const unsigned char style_css_start[] asm("_binary_style_css_start");
    extern const unsigned char style_css_end[] asm("_binary_style_css_end");

    const char* uri = req->uri;

    if (strcmp(uri, "/index.html") == 0 ||
        strcmp(uri, "/") == 0) {
        const size_t chunk_size = index_html_end - index_html_start;
        httpd_resp_set_type(req, "text/html");
        httpd_resp_send_chunk(req, (const char*)index_html_start, chunk_size);
        httpd_resp_sendstr_chunk(req, NULL);
    } else if (strcmp(uri, "/app.js") == 0) {
        const size_t chunk_size = app_js_end - app_js_start;
        httpd_resp_set_type(req, "text/javascript");
        httpd_resp_send_chunk(req, (const char*)app_js_start, chunk_size);
        httpd_resp_sendstr_chunk(req, NULL);
    } else if (strcmp(uri, "/style.css") == 0) {
        const size_t chunk_size = style_css_end - style_css_start;
        httpd_resp_set_type(req, "text/css");
        httpd_resp_send_chunk(req, (const char*)style_css_start, chunk_size);
        httpd_resp_sendstr_chunk(req, NULL);
    } else {
        httpd_resp_set_type(req, "text/plain");
        httpd_resp_sendstr_chunk(req, "non exist URI");
        httpd_resp_sendstr_chunk(req, NULL);
    }

    return ESP_OK;
}

// Helper function for appending strings to HTTP response
static void httpd_append_str(void* target, const char* str)
{
    httpd_req_t* req = (httpd_req_t*)target;
    httpd_resp_sendstr_chunk(req, str);
}

static esp_err_t layouts_json(httpd_req_t* req)
{
    httpd_resp_set_type(req, "application/json");
    esp_err_t err = generate_layouts_json(httpd_append_str, req);
    httpd_resp_sendstr_chunk(req, NULL);
    return err;
}

static esp_err_t keycodes_json(httpd_req_t* req)
{
    httpd_resp_set_type(req, "application/json");
    esp_err_t err = generate_keycodes_json(httpd_append_str, req);
    httpd_resp_sendstr_chunk(req, NULL);
    return err;
}

static esp_err_t update_keymap(httpd_req_t* req)
{
    httpd_resp_set_type(req, "text/plain");
    esp_err_t ret = ESP_OK;
    cJSON* root = NULL;

    // Use the improved parse_http_req function to parse the HTTP request
    ret = parse_http_req(req, &root);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to parse HTTP request: %s", esp_err_to_name(ret));
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Failed to parse HTTP request");
        return ret;
    }

    // Use the new utility function to handle layout updates
    ret = update_layout_from_json(root);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to update layouts: %s", esp_err_to_name(ret));
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to update layouts");
    } else {
        // Extract version for logging purposes
        cJSON* version = cJSON_GetObjectItem(root, "version");
        if (version && cJSON_IsNumber(version)) {
            uint64_t new_version = (uint64_t)version->valuedouble;
            ESP_LOGI(TAG, "Successfully updated keymap to version %llu", (unsigned long long)new_version);
        } else {
            ESP_LOGI(TAG, "Successfully updated keymap");
        }
        httpd_resp_sendstr(req, "Keymap updated successfully");
    }

    if (root) cJSON_Delete(root);
    return ret;
}

static esp_err_t reset_keymap(httpd_req_t* req)
{
    httpd_resp_set_type(req, "text/plain");

    cJSON* root = NULL;
    esp_err_t err = parse_http_req(req, &root);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to parse reset keymap request: %s", esp_err_to_name(err));
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to parse body of reset keymap request");
        return err;
    }

    cJSON* pos = cJSON_GetObjectItem(root, "positions");
    if (!pos || !cJSON_IsArray(pos) || cJSON_GetArraySize(pos) != 0) {
        cJSON_Delete(root);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "no positions in json");
        return ESP_ERR_INVALID_ARG;
    }

    cJSON* kc  = cJSON_GetObjectItem(root, "keycodes");
    if (!kc || !cJSON_IsArray(kc) || cJSON_GetArraySize(kc) != 0) {
        cJSON_Delete(root);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "no keycodes in json");
        return ESP_ERR_INVALID_ARG;
    }

    cJSON_Delete(root);

    // Use the common API to reset the keymap
    err = nvs_reset_layouts();
    if (err != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "reset fail");
        return ESP_FAIL;
    }

    httpd_resp_sendstr_chunk(req, NULL);
    return ESP_OK;
}

static esp_err_t upload_bin_file(httpd_req_t* req)
{
    int file_len = req->content_len;
    int accumu_len = 0;
    char str_buf[26];

    ESP_LOGI(TAG, "Starting OTA ...");

    const esp_partition_t* update_partition = NULL;

    update_partition = esp_ota_get_next_update_partition(NULL);
    if (!update_partition) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "no available update partition");
        return ESP_FAIL;
    }

    esp_ota_handle_t update_handle = 0;
    esp_err_t err = esp_ota_begin(update_partition, OTA_WITH_SEQUENTIAL_WRITES, &update_handle);
    if (err != ESP_OK) {
        esp_ota_abort(update_handle);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "can't begin ota");
        return ESP_FAIL;
    }

    while (accumu_len < file_len) {
        int recieved = httpd_req_recv(req, recv_buf, sizeof(recv_buf));
        if (recieved <= 0) {
            if (recieved == HTTPD_SOCK_ERR_TIMEOUT) {
                continue;
            }
            ESP_LOGE("HTTPD", "fail to recieve file");
            esp_ota_abort(update_handle);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "fail to recieve file");
            return ESP_FAIL;
        }

        err = esp_ota_write(update_handle, recv_buf, recieved);
        if (err != ESP_OK) {
            esp_ota_abort(update_handle);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "fail to write ota data");
            return ESP_FAIL;
        }

        accumu_len += recieved;
    }

    if (accumu_len != file_len) {
        esp_ota_abort(update_handle);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "recieve more data than expected");
        return ESP_FAIL;
    }

    err = esp_ota_end(update_handle);
    if (err !=  ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "can't end ota successfully");
        return ESP_FAIL;
    }

    err = esp_ota_set_boot_partition(update_partition);
    if (err != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "fail to set boot partition");
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "text/plain");
    snprintf(str_buf, sizeof(str_buf), "File size is %d.", accumu_len);
    httpd_resp_sendstr_chunk(req, str_buf);
    httpd_resp_sendstr_chunk(req, "<p style=\"color:Red\">Will reboot keyboard ...</p>");
    httpd_resp_sendstr_chunk(req, NULL);

    vTaskDelay(1500 / portTICK_PERIOD_MS);
    esp_restart();

    return ESP_OK;
}

static esp_err_t get_device_status(httpd_req_t* req)
{
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr_chunk(req, "{\n");

    // boot partition
    const esp_partition_t* curr_part = esp_ota_get_running_partition();
    httpd_resp_sendstr_chunk(req, "\"boot_partition\" : \"");
    httpd_resp_sendstr_chunk(req, curr_part->label);
    httpd_resp_sendstr_chunk(req, "\",\n");

    httpd_resp_sendstr_chunk(req, "\"wifi_state\" : \"");
    httpd_resp_sendstr_chunk(req, wifi_mode_to_str(get_wifi_mode()));
    httpd_resp_sendstr_chunk(req, "\",\n");

    // ble_state
    httpd_resp_sendstr_chunk(req, "\"ble_state\" : ");
    httpd_resp_sendstr_chunk(req, is_ble_enabled() ? "true,\n" : "false,\n");

    // usb_state
    httpd_resp_sendstr_chunk(req, "\"usb_state\" : ");
    httpd_resp_sendstr_chunk(req, is_usb_enabled() ? "true,\n" : "false,\n");

    char str_buf[25];
    snprintf(str_buf, sizeof(str_buf), "%llu", s_init_version);
    // start_time
    httpd_resp_sendstr_chunk(req, "\"init_version\" : \"");
    httpd_resp_sendstr_chunk(req, str_buf);
    httpd_resp_sendstr_chunk(req, "\"\n");

    httpd_resp_sendstr_chunk(req, "}");
    httpd_resp_sendstr_chunk(req, NULL);

    return ESP_OK;
}

static esp_err_t modify_functions(httpd_req_t* req)
{
    const char* prefix = "/api/switches/";
    const char* last_token = &req->uri[strlen(prefix)];
    esp_err_t ret;
    const char* err_str = "Oops. Something is wrong.";

    cJSON* root = NULL;
    esp_err_t err = parse_http_req(req, &root);
    if (err != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "fail to parse http request");
        return err;
    }

    if (strcmp(last_token, "WiFi") == 0) {
        const char* mode = NULL;
        const char* ssid = NULL;
        const char* passwd = NULL;

        cJSON* modeItem = cJSON_GetObjectItem(root, "mode");
        if (modeItem) {
            mode = cJSON_GetStringValue(modeItem);
        }

        cJSON* ssidItem = cJSON_GetObjectItem(root, "ssid");
        if (ssidItem) {
            ssid = cJSON_GetStringValue(ssidItem);
        }

        cJSON* passwdItem = cJSON_GetObjectItem(root, "passwd");
        if (passwdItem) {
            passwd = cJSON_GetStringValue(passwdItem);
        }

        if (mode == NULL ||
            ((strcmp(mode, "closed") != 0) &&
             (ssidItem == NULL || passwdItem == NULL))) {
            err_str = "incomplete parameters";
            ret = ESP_ERR_INVALID_ARG;
            goto err_out;
        }

        ESP_GOTO_ON_ERROR(update_wifi_mode(str_to_wifi_mode(mode), ssid, passwd), err_out, TAG, "fail to update wifi state");
    } else if (strcmp(last_token, "BLE") == 0) {
        cJSON_bool enabled = false;
        const char* name = NULL;

        cJSON* enabledItem = cJSON_GetObjectItem(root, "enabled");
        if (enabledItem) {
            enabled = cJSON_IsTrue(enabledItem);
        }

        cJSON* nameItem = cJSON_GetObjectItem(root, "name");
        if (nameItem) {
            if (!enabledItem) enabled = true;
            name = cJSON_GetStringValue(nameItem);
        }

        if (enabledItem || nameItem) {
            ESP_GOTO_ON_ERROR(update_ble_state(enabled, name), err_out, TAG, "fail to update ble state");
        }
    } else if (strcmp(last_token, "USB") == 0) {
        cJSON_bool enabled = false;

        cJSON* enabledItem = cJSON_GetObjectItem(root, "enabled");
        if (enabledItem) {
            enabled = cJSON_IsTrue(enabledItem);

            ESP_GOTO_ON_ERROR(update_usb_state(enabled), err_out, TAG, "fail to update usb state");
        }
    } else {
        ESP_LOGE(TAG, "unkown url %s", last_token);
        ret = ESP_ERR_INVALID_ARG;
        goto err_out;
    }

    cJSON_Delete(root);
    httpd_resp_sendstr_chunk(req, NULL);
    return ESP_OK;

err_out:
    cJSON_Delete(root);
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, err_str);
    return ret;
}

/**
 * @brief Escape special characters in a string for JSON output
 *
 * This function escapes all characters that need to be escaped according to JSON specification:
 * - " (quotation mark) -> \"
 * - \ (backslash) -> \\
 * - / (forward slash) -> \/ (optional but included)
 * - \b (backspace) -> \b
 * - \f (form feed) -> \f
 * - \n (line feed) -> \n
 * - \r (carriage return) -> \r
 * - \t (horizontal tab) -> \t
 * - Control characters (0x00-0x1F) -> \uXXXX
 *
 * @param content The string to escape (modified in place)
 * @param max_size Maximum size of the buffer including null terminator
 */
static void escape_string(char* content, int max_size)
{
    int len = strlen(content);
    int i = 0;

    while (i < len && content[i] != '\0') {
        unsigned char c = (unsigned char)content[i];
        int escape_len = 0;
        char escape_seq[7]; // Maximum needed for \uXXXX + null terminator

        // Determine the escape sequence needed
        switch (c) {
        case '"':
            escape_seq[0] = '\\';
            escape_seq[1] = '"';
            escape_len = 2;
            break;
        case '\\':
            escape_seq[0] = '\\';
            escape_seq[1] = '\\';
            escape_len = 2;
            break;
        case '/':
            // Optional: JSON allows forward slash to be escaped
            escape_seq[0] = '\\';
            escape_seq[1] = '/';
            escape_len = 2;
            break;
        case '\b':
            escape_seq[0] = '\\';
            escape_seq[1] = 'b';
            escape_len = 2;
            break;
        case '\f':
            escape_seq[0] = '\\';
            escape_seq[1] = 'f';
            escape_len = 2;
            break;
        case '\n':
            escape_seq[0] = '\\';
            escape_seq[1] = 'n';
            escape_len = 2;
            break;
        case '\r':
            escape_seq[0] = '\\';
            escape_seq[1] = 'r';
            escape_len = 2;
            break;
        case '\t':
            escape_seq[0] = '\\';
            escape_seq[1] = 't';
            escape_len = 2;
            break;
        default:
            // No escaping needed for this character
            i++;
            continue; // Skip the escape sequence processing
        }

        // Check if we have enough space for the escape sequence (including null terminator)
        if (len + escape_len - 1 >= max_size - 1) {
            // Not enough space, truncate here
            content[i] = '\0';
            return;
        }

        // Make room for the escape sequence by moving the rest of the string
        // Move from position i+1 to end of string, total length is (len - i - 1) + 1 for null terminator
        memmove(&content[i + escape_len], &content[i + 1], len - i);
        len += escape_len - 1;

        // Insert the escape sequence
        for (int j = 0; j < escape_len; j++) {
            content[i + j] = escape_seq[j];
        }

        i += escape_len;
    }
}

static esp_err_t get_macro_content(httpd_req_t* req)
{
    const char* prefix = "/api/macro/";
    const char* last_token = &req->uri[strlen(prefix)];
    char* content = NULL;
    esp_err_t ret;

    uint16_t keycode;
    ESP_GOTO_ON_ERROR(parse_macro_name(last_token, &keycode), err_out, TAG, "error macro name %s", last_token);

    content = malloc(MACRO_STR_MAX_LEN);
    if (!content) {
        ret = ESP_ERR_NO_MEM;
        ESP_LOGE(TAG, "not enough memory to process request %s", req->uri);
        goto err_out;
    }

    ESP_GOTO_ON_ERROR(get_macro_str(keycode, content, MACRO_STR_MAX_LEN), err_out, TAG, "fail to get macro content %s", last_token);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr_chunk(req, "{\"");
    httpd_resp_sendstr_chunk(req, last_token);
    httpd_resp_sendstr_chunk(req, "\" : \"");
    if (strlen(content) > 0) {
        escape_string(content, MACRO_STR_MAX_LEN);
        httpd_resp_sendstr_chunk(req, content);
    }
    httpd_resp_sendstr_chunk(req, "\"}");

    free(content);
    httpd_resp_sendstr_chunk(req, NULL);
    return ESP_OK;

err_out:
    if (content) free(content);
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Oops. Something is wrong.");
    return ret;
}

static esp_err_t set_macro_content(httpd_req_t* req)
{
    const char* prefix = "/api/macro/";
    const char* last_token = &req->uri[strlen(prefix)];
    esp_err_t ret;

    cJSON* root = NULL;
    ret = parse_http_req(req, &root);
    if (ret != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "fail to parse http request");
        return ret;
    }

    uint16_t keycode;
    ESP_GOTO_ON_ERROR(parse_macro_name(last_token, &keycode), err_out, TAG, "error macro name %s", last_token);

    cJSON* contentItem = cJSON_GetObjectItem(root, last_token);
    if (!contentItem) {
        ESP_LOGE(TAG, "There is no field %s in the input", last_token);
        goto err_out;
    }

    ESP_GOTO_ON_ERROR(set_macro_str(keycode, contentItem->valuestring), err_out, TAG, "fail to set macro %s string", last_token);

    cJSON_Delete(root);
    httpd_resp_sendstr_chunk(req, NULL);
    return ESP_OK;

err_out:
    cJSON_Delete(root);
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Oops. Something is wrong.");
    return ret;
}

static struct {
    const char*    uri;
    httpd_method_t method;
    esp_err_t (*handler)(httpd_req_t* req);
    void*          user_ctx;
} handler_uris[] = {
    /* URI handler for getting uploaded files */
    {"/api/layouts",           HTTP_GET,      layouts_json,           NULL},
    {"/api/keycodes",          HTTP_GET,      keycodes_json,          NULL},
    {"/api/keymap",            HTTP_PUT,      update_keymap,          NULL},
    {"/api/keymap",            HTTP_POST,     reset_keymap,           NULL},
    {"/api/device-status",     HTTP_GET,      get_device_status,      NULL},
    {"/upload/bin_file",       HTTP_POST,     upload_bin_file,        NULL},
    {"/api/switches/*",        HTTP_PUT,      modify_functions,       NULL},
    {"/api/macro/*",           HTTP_GET,      get_macro_content,      NULL},
    {"/api/macro/*",           HTTP_PUT,      set_macro_content,      NULL},
    {"/*",                     HTTP_GET,      serve_static_files,     NULL}
};

esp_err_t start_file_server()
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    // The default max handler number is 8
    config.max_uri_handlers = ARRAY_LEN(handler_uris);

    s_init_version = esp_random();

    // match uri by strncmp
    config.uri_match_fn = httpd_uri_match_wildcard;

    ESP_LOGI(TAG, "Starting HTTP Server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start file server!");
        return ESP_FAIL;
    }

    for (int i = 0; i < ARRAY_LEN(handler_uris); i++) {
        httpd_uri_t uri = {
            .uri      = handler_uris[i].uri,
            .method   = handler_uris[i].method,
            .handler  = handler_uris[i].handler,
            .user_ctx = handler_uris[i].user_ctx
        };
        httpd_register_uri_handler(server, &uri);
    }

    return ESP_OK;
}
