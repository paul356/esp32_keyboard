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
#define MAX_BUF_SIZE 2048

static time_t s_init_version;
static char recv_buf[MAX_BUF_SIZE];

static esp_err_t parse_http_req(httpd_req_t* req, cJSON** root)
{
    size_t buf_len = req->content_len;
    char* body = (char*)malloc(buf_len);
    if (!body) {
        return ESP_ERR_NO_MEM;
    }

    int ret = httpd_req_recv(req, body, buf_len);
    if (ret <= 0) {
        free(body);
        return ESP_ERR_INVALID_ARG;
    }

    *root = cJSON_ParseWithLength(body, buf_len);
    free(body);
    if (*root == NULL) {
        return ESP_ERR_NO_MEM;
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

    int total_len = req->content_len;
    if(total_len >= MAX_BUF_SIZE){
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "content too long");
        return ESP_FAIL;
    }

    memset(recv_buf, 0, MAX_BUF_SIZE);
    int received = 0;
    int cur_len  = 0;
    while(cur_len < total_len){
        received = httpd_req_recv(req, recv_buf + cur_len, total_len);
        if(received <= 0){
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to post control value");
            return ESP_FAIL;
        }
        cur_len += received;
    }
    recv_buf[total_len] = '\0';

    root = cJSON_Parse(recv_buf);
    if(root == NULL){
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "invalid json");
        return ESP_FAIL;
    }

    cJSON* layer = cJSON_GetObjectItem(root, "layer");
    if(layer == NULL){
        ret = ESP_FAIL;
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "'layer' is invalid");
        goto cleanup;
    }

    const char* layer_name = layer->valuestring;

    cJSON* positions = cJSON_GetObjectItem(root, "positions");
    if((positions == NULL) || (cJSON_GetArraySize(positions) == 0)){
        ret = ESP_FAIL;
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "'positions' is invalid");
        goto cleanup;
    }

    cJSON* keycodes = cJSON_GetObjectItem(root, "keycodes");
    if((keycodes == NULL) || (cJSON_GetArraySize(keycodes) == 0)){
        ret = ESP_FAIL;
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "'keycodes' is invalid");
        goto cleanup;
    }

    if(cJSON_GetArraySize(positions) != cJSON_GetArraySize(keycodes)){
        ret = ESP_FAIL;
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "length of 'positions' not match with 'keycodes'");
        goto cleanup;
    }

    uint8_t layers_count;
    uint32_t rows, cols;
    nvs_get_keymap_info(&layers_count, &rows, &cols);
    uint32_t layout_len = rows * cols;

    // Use a temporary buffer to hold the modified layer data
    uint16_t temp_layer[MATRIX_ROWS * MATRIX_COLS];
    ret = nvs_get_layer(layer_name, temp_layer, (uint16_t)layout_len);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get layer %s, err=%d", layer_name, ret);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to get layer");
        goto cleanup;
    }

    // Process all the position/keycode pairs
    for (uint16_t i = 0; i < cJSON_GetArraySize(positions); ++i)
    {
        uint16_t pos = (uint16_t)cJSON_GetArrayItem(positions, i)->valueint;
        const char* keyname = cJSON_GetArrayItem(keycodes, i)->valuestring;
        ESP_LOGI(TAG, "%s: pos = '%hu', keycode = '%s'", __FUNCTION__, pos, keyname);

        if (pos >= layout_len) {
            ESP_LOGE(TAG, "%s: invalid position %hu", __FUNCTION__, pos);
            continue;
        }

        uint16_t keycode;
        // Parse the keycode name
        esp_err_t err = parse_full_key_name(keyname, &keycode);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Can't parse %s, err=%d", keyname, err);
            continue;
        }

        temp_layer[pos] = keycode;
    }

    // Save the keymap to NVS
    ret = nvs_save_layer(layer_name, temp_layer, layout_len);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save layer %s, err=%d", layer_name, ret);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to save layer");
    }

cleanup:
    if (root) {
        cJSON_Delete(root);
    }

    httpd_resp_sendstr_chunk(req, NULL);
    return ret;
}

static esp_err_t reset_keymap(httpd_req_t* req)
{
    httpd_resp_set_type(req, "text/plain");

    size_t buf_len = req->content_len;
    char* body = (char*)malloc(buf_len);
    if (!body) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, NULL);
        return ESP_ERR_NO_MEM;
    }

    int ret = httpd_req_recv(req, body, buf_len);
    if (ret <= 0) {
        free(body);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, NULL);
        return ESP_ERR_INVALID_ARG;
    }

    cJSON* root = cJSON_ParseWithLength(body, buf_len);
    free(body);
    if (!root) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "invalid json");
        return ESP_ERR_NO_MEM;
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
    esp_err_t err = nvs_reset_layouts();
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
        int recieved = httpd_req_recv(req, recv_buf, MAX_BUF_SIZE);
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

        ESP_GOTO_ON_ERROR(update_wifi_state(str_to_wifi_mode(mode), ssid, passwd), err_out, TAG, "fail to update wifi state");
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

static void escape_string(char* content, int max_size)
{
    int len = strlen(content);

    int i = 0;
    while (content[i] != '\0') {
        switch (content[i]) {
        case '\n':
            if (len + 1 >= max_size) {
                return;
            }
            memmove(&content[i+2], &content[i+1], len - i);
            len += 1;
            content[i] = '\\';
            content[i+1] = 'n';
            i += 2;
            break;
        case '"':
            if (len + 1 >= max_size) {
                return;
            }
            memmove(&content[i+2], &content[i+1], len - i);
            len += 1;
            content[i] = '\\';
            content[i+1] = '"';
            i += 2;
            break;
        default:
            i++;
            break;
        }
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
