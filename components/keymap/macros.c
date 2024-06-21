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
#include "nvs_funcs.h"
#include "macros.h"
#include "quantum.h"

#define TAG "MACROS"
#define MACRO_NAMESPACE "MACROS"
#define MACRO_NAME_PREFIX "MACRO"

esp_err_t set_macro_str(uint16_t keycode, char* buf)
{
    int blob_len = strlen(buf) + 1;

    if (blob_len > MACRO_STR_MAX_LEN) {
        ESP_LOGE(TAG, "macro string is too long, len=%d", blob_len);
        return ESP_FAIL;
    }

    keycode -= MACRO_CODE_MIN;

    char scratch[8];
    int len = snprintf(scratch, sizeof(scratch), MACRO_NAME_PREFIX "%d", keycode);
    if (len >= sizeof(scratch) || len < 0) {
        ESP_LOGE(TAG, "keycode is too large, keycode=%u ", keycode);
        return ESP_FAIL;
    }

    return nvs_write_blob(MACRO_NAMESPACE, scratch, buf, blob_len);
}

esp_err_t get_macro_str(uint16_t keycode, char* buf, int buf_len)
{
    keycode -= MACRO_CODE_MIN;

    char scratch[8];
    int len = snprintf(scratch, sizeof(scratch), MACRO_NAME_PREFIX "%d", keycode);
    if (len >= sizeof(scratch) || len < 0) {
        ESP_LOGE(TAG, "keycode is too large, keycode=%u ", keycode);
        return ESP_FAIL;
    }

    size_t buf_size = buf_len;
    esp_err_t err = nvs_read_blob(MACRO_NAMESPACE, scratch, buf, &buf_size);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        len = snprintf(buf, buf_len, "none");
        if (len >= buf_len) {
            return ESP_FAIL;
        } else {
            return ESP_OK;
        }
    }

    return err;
}

static inline int macro_idx(uint16_t keycode)
{
    return keycode - MACRO_CODE_MIN;
}

esp_err_t get_macro_name(uint16_t keycode, char* buf, int buf_len)
{
    int required_size = snprintf(buf, buf_len, MACRO_NAME_PREFIX "%d", macro_idx(keycode));
    if (required_size + 1 > buf_len) {
        return ESP_ERR_INVALID_SIZE;
    } else {
        return ESP_OK;
    }
}

esp_err_t parse_macro_name(const char* macro_name, uint16_t* keycode)
{
    int name_len = strlen(macro_name);
    int prefix_len = strlen(MACRO_NAME_PREFIX);
    static int max_idx_len = 0;

    if (max_idx_len == 0) {
        char scratch[8];
        max_idx_len = snprintf(scratch, sizeof(scratch), MACRO_NAME_PREFIX "%d", MACRO_CODE_NUM - 1);
    }

    if (name_len <= prefix_len ||
        name_len > strlen(MACRO_NAME_PREFIX) + max_idx_len) {
        return ESP_ERR_INVALID_ARG;
    }

    if (strncmp(macro_name, MACRO_NAME_PREFIX, prefix_len) != 0) {
        return ESP_ERR_INVALID_ARG;
    }

    long idx = strtol(&macro_name[prefix_len], NULL, 10);
    if (idx < 0 || idx >= MACRO_CODE_NUM) {
        return ESP_ERR_INVALID_ARG;
    }

    *keycode = (uint16_t)(idx + MACRO_CODE_MIN);
    return ESP_OK;
}

esp_err_t process_macro_code(uint16_t keycode)
{
    char* content = malloc(MACRO_STR_MAX_LEN);
    if (!content) {
        SEND_STRING("no memory!");
    } else {
        esp_err_t err = get_macro_str(keycode, content, MACRO_STR_MAX_LEN);
        if (err != ESP_OK) {
            SEND_STRING("Oops...");
        } else {
            SEND_STRING(content);
        }
        free(content);
    }

    return ESP_OK;
}


