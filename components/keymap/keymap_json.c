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
    
    nvs_get_keymap_info(&layers, &rows, &cols);

    append_str(target, "{\"layouts\":{\n");

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