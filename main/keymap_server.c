#include "esp_http_server.h"
#include "esp_log.h"

#define TAG "[HTTPD]"

static esp_err_t front_page(httpd_req_t* req)
{
    httpd_resp_set_type(req, "text/plain");
    httpd_resp_sendstr_chunk(req, "hello keymap");
    httpd_resp_sendstr_chunk(req, NULL);
    return ESP_OK;
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
    httpd_uri_t get_uri = {
        .uri       = "/*",  // Match all URIs of type /path/to/file
        .method    = HTTP_GET,
        .handler   = front_page,
        .user_ctx  = NULL    // Pass server data as context
    };
    httpd_register_uri_handler(server, &get_uri);

    return ESP_OK;
}
