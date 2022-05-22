#include "esp_http_server.h"
#include "esp_log.h"
#include "keyboard_config.h"
#include "key_definitions.h"
#include "nvs_keymaps.h"

#define TAG "[HTTPD]"

static esp_err_t front_page(httpd_req_t* req)
{
    httpd_resp_set_type(req, "text/plain");
    httpd_resp_sendstr_chunk(req, "non exist URI");
    httpd_resp_sendstr_chunk(req, NULL);

    return ESP_OK;
}

static esp_err_t layouts_json(httpd_req_t* req)
{
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr_chunk(req, "{\"layouts\":{");
    for (int i = 0; i < layers_num; i ++) {
        httpd_resp_sendstr_chunk(req, "\"");
        httpd_resp_sendstr_chunk(req, layer_names_arr[i]);
        httpd_resp_sendstr_chunk(req, "\":[");

        for (int j = 0; j < MATRIX_ROWS; j++) {
            for (int k = 0; k < KEYMAP_COLS; k++) {
                uint16_t key_code = layouts[i][j][k];
                httpd_resp_sendstr_chunk(req, "\"");
                const char* keyName = GetKeyCodeName(key_code);
                if (keyName != NULL) {
                    httpd_resp_sendstr_chunk(req, keyName);
                } else {
                    httpd_resp_sendstr_chunk(req, GetKeyCodeName(KC_NONE));
                }
                httpd_resp_sendstr_chunk(req, "\"");
                if (j != MATRIX_ROWS - 1 || k != KEYMAP_COLS - 1) {
                    httpd_resp_sendstr_chunk(req, ",");
                }
            }
        }
        
        httpd_resp_sendstr_chunk(req, "]");
        if (i != layers_num - 1) {
            httpd_resp_sendstr_chunk(req, ",");
        }
    }
    httpd_resp_sendstr_chunk(req, "}}");
    httpd_resp_sendstr_chunk(req, NULL);

    return ESP_OK;
}

static esp_err_t keycodes_json(httpd_req_t* req)
{
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr_chunk(req, "{\"keycodes\":[");
    uint16_t keyCodeNum = GetKeyCodeNum();
    bool firstKey = true;
    for (uint16_t kc = 0; kc < keyCodeNum; kc++) {
        const char* keyName = GetKeyCodeName(kc);
        if (keyName != NULL && keyName[0] != '\0') {
            if (firstKey) {
                httpd_resp_sendstr_chunk(req, "\"");
                firstKey = false;
            } else {
                httpd_resp_sendstr_chunk(req, ",\"");
            }
            httpd_resp_sendstr_chunk(req, keyName);
            httpd_resp_sendstr_chunk(req, "\"");
        }
    }
    httpd_resp_sendstr_chunk(req, "]}");
    httpd_resp_sendstr_chunk(req, NULL);
    
    return ESP_OK;
}

static esp_err_t update_keymap(httpd_req_t* req)
{
    return ESP_FAIL;
}

static esp_err_t reset_keymap(httpd_req_t* req)
{
    return ESP_FAIL;
}

esp_err_t start_file_server()
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    // match uri by strncmp
    config.uri_match_fn = httpd_uri_match_wildcard;

    ESP_LOGI(TAG, "Starting HTTP Server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start file server!");
        return ESP_FAIL;
    }

    /* URI handler for getting uploaded files */
    httpd_uri_t layouts_uri = {
        .uri       = "/api/layouts",  // Match all URIs of type /path/to/file
        .method    = HTTP_GET,
        .handler   = layouts_json,
        .user_ctx  = NULL    // Pass server data as context
    };

    httpd_register_uri_handler(server, &layouts_uri);

    httpd_uri_t keycodes_uri = {
        .uri       = "/api/keycodes",  // Match all URIs of type /path/to/file
        .method    = HTTP_GET,
        .handler   = keycodes_json,
        .user_ctx  = NULL    // Pass server data as context
    };

    httpd_register_uri_handler(server, &keycodes_uri);

    /* URI handler for getting uploaded files */
    httpd_uri_t update_uri = {
        .uri       = "/api/keymap",  // Match all URIs of type /path/to/file
        .method    = HTTP_PUT,
        .handler   = update_keymap,
        .user_ctx  = NULL    // Pass server data as context
    };

    httpd_register_uri_handler(server, &update_uri);

    /* URI handler for getting uploaded files */
    httpd_uri_t reset_uri = {
        .uri       = "/api/keymap",  // Match all URIs of type /path/to/file
        .method    = HTTP_POST,
        .handler   = reset_keymap,
        .user_ctx  = NULL    // Pass server data as context
    };

    httpd_register_uri_handler(server, &reset_uri);
    
    /* URI handler for getting uploaded files */
    httpd_uri_t get_uri = {
        .uri       = "/*",  // Match all URIs of type /path/to/file
        .method    = HTTP_GET,
        .handler   = front_page,
        .user_ctx  = NULL    // Pass server data as context
    };

    httpd_register_uri_handler(server, &get_uri);

    return ESP_OK;
}
