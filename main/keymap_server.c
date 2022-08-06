#include "esp_http_server.h"
#include "esp_log.h"
#include "keyboard_config.h"
#include "key_definitions.h"
#include "nvs_keymaps.h"
#include "nvs_funcs.h"
#include "cJSON.h"
#include "keymap.h"

#define TAG "[HTTPD]"

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

static esp_err_t layouts_json(httpd_req_t* req)
{
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr_chunk(req, "{\"layouts\":{");
    for (int i = 0; i < layers_num; i ++) {
        httpd_resp_sendstr_chunk(req, "\"");
        httpd_resp_sendstr_chunk(req, layer_names_arr[i]);
        httpd_resp_sendstr_chunk(req, "\":[");

        for (int j = 0; j < MATRIX_ROWS; j++) {
            for (int k = 0; k < MATRIX_COLS; k++) {
                uint16_t key_code = layouts[i][j][k];
                httpd_resp_sendstr_chunk(req, "\"");
                const char* keyName = GetKeyCodeName(key_code);
                if (keyName != NULL) {
                    httpd_resp_sendstr_chunk(req, keyName);
                } else {
                    httpd_resp_sendstr_chunk(req, GetKeyCodeName(KC_NONE));
                }
                httpd_resp_sendstr_chunk(req, "\"");
                if (j != MATRIX_ROWS - 1 || k != MATRIX_COLS - 1) {
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

#define MAX_BUF_SIZE 1024
char recv_buf[MAX_BUF_SIZE];
static esp_err_t update_keymap(httpd_req_t* req)
{
    httpd_resp_set_type(req, "text/plain");

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

    cJSON *root = cJSON_Parse(recv_buf);
    if(root == NULL){
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "invalid json");
        return ESP_FAIL;
    }

    cJSON *layer = cJSON_GetObjectItem(root, "layer");
    if(layer == NULL){
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "'layer' is invalid");
        return ESP_FAIL;
    }

    const char* layer_name = layer->valuestring;    
    int layer_index = -1;
    for(uint16_t i = 0; i < LAYERS; ++i){
        if(strcmp(layer_name, default_layout_names[i]) == 0) layer_index = i;
    }
    if(layer_index < 0){
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "'layer' should be one of ['QWERTY', 'NUM', 'Plugins']");
        return ESP_FAIL;
    }

    cJSON *positions = cJSON_GetObjectItem(root, "positions");
    if((positions == NULL) || (cJSON_GetArraySize(positions) == 0)){
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "'positions' is invalid");
        return ESP_FAIL;
    }

    cJSON *keycodes = cJSON_GetObjectItem(root, "keycodes");
    if((keycodes == NULL) || (cJSON_GetArraySize(keycodes) == 0)){
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "'keycodes' is invalid");
        return ESP_FAIL;
    }

    if(cJSON_GetArraySize(positions) != cJSON_GetArraySize(keycodes)){
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "length of 'positions' not match with 'keycodes'");
        return ESP_FAIL;
    }

    uint16_t temp_layer[MATRIX_ROWS * MATRIX_COLS];
    memcpy(temp_layer, layouts[layer_index], sizeof(uint16_t) * MATRIX_ROWS * MATRIX_COLS);
    for(uint16_t i = 0; i < cJSON_GetArraySize(positions); ++i){
        uint16_t pos = (uint16_t)cJSON_GetArrayItem(positions, i)->valueint;
        const char* keyname = cJSON_GetArrayItem(keycodes, i)->valuestring;
        int keycode = GetKeyCodeWithName(keyname);
        ESP_LOGI(TAG, "PUT: pos = '%d', keycode = '%d'", pos, keycode);
        if(keycode < 0) continue; // ignore invalid keycode
        ESP_LOGI(TAG, "PUT: keycode = '%d' -> '%d'", temp_layer[pos], keycode);
        temp_layer[pos] = keycode;
    }

    // save layout
    memcpy(layouts[layer_index], temp_layer, sizeof(uint16_t) * MATRIX_ROWS * MATRIX_COLS);
    nvs_write_layout(temp_layer, layer_name);

    cJSON_Delete(root);

    httpd_resp_sendstr_chunk(req, NULL);
    return ESP_OK;
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

    for (int i = 0; i < LAYERS; i++) {
        // save layout
        memcpy(&layouts[i][0][0], &_LAYERS[i][0][0], sizeof(uint16_t) * MATRIX_ROWS * MATRIX_COLS);
        nvs_write_layout(&layouts[i][0][0], default_layout_names[i]);
    }

    
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
    httpd_uri_t layouts_uri = {
        .uri       = "/api/layouts",  // return keyboard layouts
        .method    = HTTP_GET,
        .handler   = layouts_json,
        .user_ctx  = NULL    // Pass server data as context
    };

    httpd_register_uri_handler(server, &layouts_uri);

    httpd_uri_t keycodes_uri = {
        .uri       = "/api/keycodes",  // return valid keycodes
        .method    = HTTP_GET,
        .handler   = keycodes_json,
        .user_ctx  = NULL    // Pass server data as context
    };

    httpd_register_uri_handler(server, &keycodes_uri);

    /* URI handler for getting uploaded files */
    httpd_uri_t update_uri = {
        .uri       = "/api/keymap",  // update keymap of specified keys
        .method    = HTTP_PUT,
        .handler   = update_keymap,
        .user_ctx  = NULL    // Pass server data as context
    };

    httpd_register_uri_handler(server, &update_uri);

    /* URI handler for getting uploaded files */
    httpd_uri_t reset_uri = {
        .uri       = "/api/keymap",  // reset keymap to default value
        .method    = HTTP_POST,
        .handler   = reset_keymap,
        .user_ctx  = NULL    // Pass server data as context
    };

    httpd_register_uri_handler(server, &reset_uri);
    
    /* URI handler for getting uploaded files */
    httpd_uri_t get_uri = {
        .uri       = "/*",  // Match all URIs of type /path/to/file
        .method    = HTTP_GET,
        .handler   = serve_static_files,
        .user_ctx  = NULL    // Pass server data as context
    };

    httpd_register_uri_handler(server, &get_uri);

    return ESP_OK;
}
