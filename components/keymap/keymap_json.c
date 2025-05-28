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

#include "esp_log.h"
#include "esp_check.h"
#include "key_definitions.h"
#include "function_key.h"
#include "layout_store.h"
#include "macros.h"
#include "keymap_json.h"

#define TAG "KEYMAP_JSON"

esp_err_t generate_layouts_json(append_str_fn_t append_str, void* target)
{
    esp_err_t err;
    uint8_t layers;
    uint32_t rows, cols;
    uint64_t version;
    char version_str[32];

    nvs_get_keymap_info(&layers, &rows, &cols);

    // Get layout version
    err = nvs_get_layout_version(&version);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get layout version: %s", esp_err_to_name(err));
        return err;
    }

    snprintf(version_str, sizeof(version_str), "%llu", (unsigned long long)version);

    append_str(target, "{\"version\":");
    append_str(target, version_str);
    append_str(target, ",\"layouts\":{\n");

    for (int i = 0; i < layers; i++) {
        append_str(target, "\"");
        append_str(target, nvs_get_layer_name(i));
        append_str(target, "\":[");

        for (int j = 0; j < rows; j++) {
            append_str(target, "\n  [");
            for (int k = 0; k < cols; k++) {
                append_str(target, "\"");

                // Use existing keymap functions
                char scratch_buf[64];
                uint16_t keycode;

                // Use direct function parameters instead of keymap_position_t
                err = nvs_get_keycode(i, j, k, &keycode);
                if (err == ESP_OK) {
                    err = get_full_key_name(keycode, scratch_buf, sizeof(scratch_buf));
                    if (err == ESP_OK) {
                        append_str(target, scratch_buf);
                    } else {
                        ESP_LOGE(TAG, "invalid key code 0x%x @[%d, %d, %d]", keycode, i, j, k);
                    }
                } else {
                    ESP_LOGE(TAG, "error getting keycode at [%d, %d, %d]: %s", i, j, k, esp_err_to_name(err));
                }

                if (err != ESP_OK) {
                    // Send default code
                    append_str(target, "____");
                }

                if (k != cols - 1) {
                    append_str(target, "\", ");
                } else {
                    append_str(target, "\"");
                }
            }
            if (j != rows - 1) {
                append_str(target, "],\n");
            } else {
                append_str(target, "]");
            }
        }

        if (i != layers - 1) {
            append_str(target, "\n],\n");
        } else {
            append_str(target, "\n]\n");
        }
    }
    append_str(target, "}}");

    return ESP_OK;
}

static void json_quantum_desc(append_str_fn_t append_str, void* target, funct_desc_t* desc)
{
    append_str(target, "{\"desc\" : \"");
    append_str(target, desc->desc);
    append_str(target, "\", ");
    append_str(target, "\"arg_types\" : [");
    bool first_key = true;
    for (int i = 0; i < desc->num_args; i++) {
        if (first_key) {
            first_key = false;
        } else {
            append_str(target, ", ");
        }

        switch (desc->arg_types[i]) {
        case LAYER_NUM:
            append_str(target, "\"layer_num\"");
            break;
        case BASIC_CODE:
            append_str(target, "\"basic_code\"");
            break;
        case MOD_BITS:
            append_str(target, "\"mod_bits\"");
            break;
        case MACRO_CODE:
            append_str(target, "\"macro_code\"");
            break;
        case FUNCTION_KEY_CODE:
            append_str(target, "\"function_key_code\"");
            break;
        }
    }
    append_str(target, "]}");
}

esp_err_t generate_keycodes_json(append_str_fn_t append_str, void* target)
{
    char scratch[16];

    append_str(target, "{\n  \"keycodes\":[");
    uint16_t keyCodeNum = get_max_basic_key_code();
    bool firstKey = true;
    for (uint16_t kc = 0; kc < keyCodeNum; kc++) {
        const char* keyName = get_basic_key_name(kc);

        if (keyName) {
            if (firstKey) {
                append_str(target, "\"");
                firstKey = false;
            } else {
                append_str(target, ", \"");
            }

            append_str(target, keyName);

            append_str(target, "\"");
        }
    }
    append_str(target, "],\n");

    append_str(target, "  \"mod_bits\":[");
    firstKey = true;
    for (int i = 0; i < get_mod_bit_num(); i++) {
        if (firstKey) {
            append_str(target, "\"");
            firstKey = false;
        } else {
            append_str(target, ", \"");
        }

        append_str(target, get_mod_bit_name(i));

        append_str(target, "\"");
    }
    append_str(target, "],\n");

    append_str(target, "  \"quantum_functs\":[");
    firstKey = true;
    for (int i = 0; i < get_total_funct_num(); i++) {
        if (firstKey) {
            firstKey = false;
        } else {
            append_str(target, ", ");
        }

        funct_desc_t desc;
        get_funct_desc(i, &desc);
        json_quantum_desc(append_str, target, &desc);
    }
    append_str(target, "],\n");

    append_str(target, "  \"macros\":[");
    firstKey = true;
    for (uint16_t code = MACRO_CODE_MIN; code <= MACRO_CODE_MAX; code++) {
        if (firstKey) {
            append_str(target, "\"");
            firstKey = false;
        } else {
            append_str(target, ", \"");
        }

        char scratch_buf[12];
        (void)get_macro_name(code, scratch_buf, sizeof(scratch_buf));
        append_str(target, scratch_buf);

        append_str(target, "\"");
    }
    append_str(target, "],\n");

    append_str(target, "  \"layer_num\":");

    // Get the number of layers from the NVS function
    uint8_t layers;
    uint32_t rows, cols;
    nvs_get_keymap_info(&layers, &rows, &cols);
    snprintf(scratch, sizeof(scratch), "%d", layers);
    append_str(target, scratch);
    append_str(target, ",\n");

    append_str(target, "  \"function_keys\":[");
    firstKey = true;
    for (uint16_t code = FUNCTION_KEY_MIN; code <= FUNCTION_KEY_MAX; code++) {
        if (firstKey) {
            append_str(target, "\"");
            firstKey = false;
        } else {
            append_str(target, ", \"");
        }

        append_str(target, get_function_key_str(code));

        append_str(target, "\"");
    }
    append_str(target, "]\n}");

    return ESP_OK;
}

esp_err_t update_layout_from_json(const cJSON *json)
{
    if (!json) {
        ESP_LOGE(TAG, "JSON input is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = ESP_OK;
    uint64_t new_version = 0;

    // Arrays for batch layout update
    uint16_t** positions = NULL;
    uint16_t** keycodes = NULL;  // These will store the parsed integer values of keycodes
    uint16_t* counts = NULL;
    const char** layer_names = NULL;  // Layer names to use directly

    // Validate required fields
    cJSON* changes = cJSON_GetObjectItem(json, "changes");
    cJSON* version = cJSON_GetObjectItem(json, "version");

    if (!changes || !cJSON_IsObject(changes) || !version || !cJSON_IsNumber(version)) {
        ESP_LOGE(TAG, "Invalid JSON format: missing or invalid 'changes' or 'version'");
        return ESP_ERR_INVALID_ARG;
    }

    // Parse version
    if (version->valuedouble > (double)UINT64_MAX) {
        ESP_LOGE(TAG, "Version value too large for uint64_t");
        return ESP_ERR_INVALID_ARG;
    }
    new_version = (uint64_t)version->valuedouble;
    ESP_LOGI(TAG, "Updating keymap to version %llu", (unsigned long long)new_version);

    // Get keyboard dimensions
    uint8_t layers_count;
    uint32_t rows, cols;
    ret = nvs_get_keymap_info(&layers_count, &rows, &cols);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get keymap info: %s", esp_err_to_name(ret));
        return ret;
    }

    // First pass: count layers and allocate memory
    // Count the number of children in the 'changes' object
    int layer_count = 0;
    cJSON* layer_entry;
    cJSON_ArrayForEach(layer_entry, changes) {
        layer_count++;
    }

    if (layer_count == 0) {
        ESP_LOGW(TAG, "No layer changes found");
        return ESP_ERR_INVALID_ARG;
    }

    // Allocate arrays for the batch update
    positions = calloc(layer_count, sizeof(uint16_t*));
    keycodes = calloc(layer_count, sizeof(uint16_t*));  // Will store the parsed integer values
    counts = calloc(layer_count, sizeof(uint16_t));
    layer_names = calloc(layer_count, sizeof(char*));  // Just need to store pointers, not copy strings

    if (!positions || !keycodes || !counts || !layer_names) {
        ESP_LOGE(TAG, "Failed to allocate memory for batch update");
        ret = ESP_ERR_NO_MEM;
        goto cleanup;
    }

    // Second pass: process each layer's changes
    int valid_layer_count = 0;
    cJSON* layer;

    // Process each layer in the changes object
    cJSON_ArrayForEach(layer, changes) {
        // Get layer name directly from JSON - for objects, the name is in 'string' field
        const char* layer_name = layer->string;
        if (!layer_name) {
            ESP_LOGW(TAG, "Missing layer name in JSON");
            continue;
        }

        // Get positions and keycodes arrays
        cJSON* pos_array = cJSON_GetObjectItem(layer, "positions");
        cJSON* kc_array = cJSON_GetObjectItem(layer, "keycodes");

        if (!pos_array || !kc_array ||
            !cJSON_IsArray(pos_array) || !cJSON_IsArray(kc_array) ||
            cJSON_GetArraySize(pos_array) != cJSON_GetArraySize(kc_array)) {
            ESP_LOGW(TAG, "Invalid positions or keycodes array for layer '%s'", layer_name);
            continue;
        }

        int pos_count = cJSON_GetArraySize(pos_array);
        if (pos_count == 0) {
            ESP_LOGD(TAG, "No changes for layer '%s'", layer_name);
            continue;
        }

        // Allocate arrays for this layer's positions and keycodes
        positions[valid_layer_count] = calloc(pos_count, sizeof(uint16_t));
        keycodes[valid_layer_count] = calloc(pos_count, sizeof(uint16_t));

        if (!positions[valid_layer_count] || !keycodes[valid_layer_count]) {
            ESP_LOGE(TAG, "Failed to allocate memory for layer '%s'", layer_name);
            ret = ESP_ERR_NO_MEM;
            goto cleanup;
        }

        // Process positions and keycodes for this layer
        int valid_changes = 0;
        for (int i = 0; i < pos_count; i++) {
            cJSON* pos = cJSON_GetArrayItem(pos_array, i);
            cJSON* keycode_item = cJSON_GetArrayItem(kc_array, i);

            // Validate position is a number
            if (!cJSON_IsNumber(pos)) {
                ESP_LOGW(TAG, "Invalid position entry at index %d", i);
                continue;
            }

            int position = pos->valueint;

            // Bounds checking for position
            if (position >= rows * cols) {
                ESP_LOGW(TAG, "Position out of range: %d", position);
                continue;
            }

            // Ensure keycode is a string
            if (!cJSON_IsString(keycode_item) || keycode_item->valuestring == NULL) {
                ESP_LOGW(TAG, "Invalid keycode entry at index %d", i);
                continue;
            }

            uint16_t keycode_value = 0; // Numeric representation of the keycode after parsing
            const char* keycode_str = keycode_item->valuestring;
            // Parse the keycode string into integer value
            if (parse_full_key_name(keycode_str, &keycode_value) != ESP_OK) {
                ESP_LOGW(TAG, "Failed to parse keycode string: %s", keycode_str);
                continue;
            }

            // Add to layer changes
            positions[valid_layer_count][valid_changes] = position;
            keycodes[valid_layer_count][valid_changes] = keycode_value; // Store the parsed value
            valid_changes++;

            ESP_LOGD(TAG, "Layer '%s': position %d, keycode 0x%04x", layer_name, position, keycode_value);
        }

        // Update the count for this layer
        if (valid_changes > 0) {
            layer_names[valid_layer_count] = layer_name;  // Store pointer to layer name
            counts[valid_layer_count] = valid_changes;
            valid_layer_count++;
            ESP_LOGI(TAG, "Layer '%s' has %d valid changes", layer_name, valid_changes);
        } else {
            // Free memory if no valid changes
            free(positions[valid_layer_count]);
            free(keycodes[valid_layer_count]);
            positions[valid_layer_count] = NULL;
            keycodes[valid_layer_count] = NULL;
        }
    }

    if (valid_layer_count == 0) {
        ESP_LOGW(TAG, "No valid layer changes found");
        ret = ESP_ERR_INVALID_ARG;
        goto cleanup;
    }

    ESP_LOGI(TAG, "Applying batch update for %d layers with version %llu",
             valid_layer_count, (unsigned long long)new_version);

    // Update layers directly by name
    ret = nvs_update_layout(new_version, valid_layer_count,
                           layer_names,
                           (const uint16_t**)positions,
                           (const uint16_t**)keycodes,
                           counts);

cleanup:
    // Free memory
    if (layer_names) free(layer_names);

    if (positions) {
        for (int i = 0; i < layer_count; i++) {
            if (positions[i]) free(positions[i]);
        }
        free(positions);
    }

    if (keycodes) {
        for (int i = 0; i < layer_count; i++) {
            if (keycodes[i]) free(keycodes[i]);
        }
        free(keycodes);
    }

    if (counts) free(counts);

    return ret;
}