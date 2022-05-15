#include "esp_http_server.h"
#include "esp_log.h"
#include "keyboard_config.h"

#define TAG "[HTTPD]"

extern char default_layout_names[LAYERS][MAX_LAYOUT_NAME_LENGTH];
extern uint16_t (*default_layouts[])[MATRIX_ROWS][KEYMAP_COLS];
extern char* key_code_name[];

static esp_err_t front_page(httpd_req_t* req)
{
    httpd_resp_set_type(req, "text/plain");
    httpd_resp_sendstr_chunk(req, "M32 main page");
    httpd_resp_sendstr_chunk(req, NULL);

    return ESP_OK;
}

static esp_err_t layouts_json(httpd_req_t* req)
{
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr_chunk(req, "{\"layouts\":{");
    for (int i = 0; i < LAYERS; i ++) {
        httpd_resp_sendstr_chunk(req, "\"");
        httpd_resp_sendstr_chunk(req, default_layout_names[i]);
        httpd_resp_sendstr_chunk(req, "\":[");

        for (int j = 0; j < MATRIX_ROWS; j++) {
            for (int k = 0; k < KEYMAP_COLS; k++) {
                uint16_t key_code = (*(default_layouts[i]))[j][k];
                httpd_resp_sendstr_chunk(req, "\"");
                if (key_code > 0xff) {
                    httpd_resp_sendstr_chunk(req, "EXTENDED");
                } else {
                    httpd_resp_sendstr_chunk(req, key_code_name[key_code]);
                }
                httpd_resp_sendstr_chunk(req, "\"");
                if (j != MATRIX_ROWS - 1 || k != KEYMAP_COLS - 1) {
                    httpd_resp_sendstr_chunk(req, ",");
                }
            }
        }
        
        httpd_resp_sendstr_chunk(req, "]");
        if (i != LAYERS - 1) {
            httpd_resp_sendstr_chunk(req, ",");
        }
    }
    httpd_resp_sendstr_chunk(req, "}}");
    httpd_resp_sendstr_chunk(req, NULL);

    return ESP_OK;
}

static esp_err_t keycodes_json(httpd_req_t* req)
{
    return ESP_FAIL;
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
