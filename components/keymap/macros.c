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

#include <string.h>
#include <ctype.h>
#include "esp_log.h"
#include "esp_err.h"
#include "nvs.h"
#include "nvs_io.h"
#include "macros.h"
#include "quantum.h"
#include "send_string_keycodes.h"

#define TAG "MACROS"
#define MACRO_NAMESPACE "MACROS"
#define MACRO_NAME_PREFIX "MACRO"

// Mapping table for user-friendly expressions to QMK representations
typedef struct {
    const char* user_expr;
    const char* qmk_expr;
} expression_mapping_t;

#define MAX_MODIFIER_DEPTH 8  // Maximum nesting depth for modifiers

static const expression_mapping_t special_key_mappings[] = {
    {"\\d", SS_TAP(X_DELETE)},
    {"\\b", SS_TAP(X_BSPACE)},
    {"\\t", SS_TAP(X_TAB)},
    {"\\)", ")"},
    {"\\e", SS_TAP(X_ESCAPE)},
    {NULL, NULL}
};

static const expression_mapping_t modifier_mappings[] = {
    {"\\lshift(", SS_DOWN(X_LSFT)},
    {"\\rshift(", SS_DOWN(X_RSFT)},
    {"\\lctrl(", SS_DOWN(X_LCTRL)},
    {"\\rctrl(", SS_DOWN(X_RCTRL)},
    {"\\lalt(", SS_DOWN(X_LALT)},
    {"\\ralt(", SS_DOWN(X_RALT)},
    {"\\lgui(", SS_DOWN(X_LGUI)},
    {"\\rgui(", SS_DOWN(X_RGUI)},
    {NULL, NULL}
};

static const expression_mapping_t modifier_up_mappings[] = {
    {")", SS_UP(X_LSFT)},
    {")", SS_UP(X_RSFT)},
    {")", SS_UP(X_LCTRL)},
    {")", SS_UP(X_RCTRL)},
    {")", SS_UP(X_LALT)},
    {")", SS_UP(X_RALT)},
    {")", SS_UP(X_LGUI)},
    {")", SS_UP(X_RGUI)},
    {NULL, NULL}
};

// Helper to check if a character is printable ASCII
static inline bool is_printable_ascii(char c) {
    // space to tilde based on table at https://www.ascii-code.com/
    return (c >= 32 && c <= 126);
}

// Convert user-friendly expressions to QMK compatible representations
esp_err_t convert_user_friendly_to_qmk(const char* user_input, char* qmk_output, size_t output_size)
{
    if (!user_input || !qmk_output || output_size == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    size_t input_len = strlen(user_input);
    size_t output_pos = 0;
    size_t input_pos = 0;

    // Stack to track enclosing modifiers (avoiding malloc)
    uint8_t modifier_stack[MAX_MODIFIER_DEPTH];
    int stack_top = -1;

    while (input_pos < input_len && output_pos < output_size - 1) {
        bool found_match = false;

        // Only check templates if current character is a backslash
        if (user_input[input_pos] == '\\') {
            // Check for special key mappings first
            for (int i = 0; special_key_mappings[i].user_expr != NULL; i++) {
                size_t expr_len = strlen(special_key_mappings[i].user_expr);
                if (strncmp(&user_input[input_pos], special_key_mappings[i].user_expr, expr_len) == 0) {
                    // Don't convert "\\)" outside of any modifier
                    if (user_input[input_pos + 1] == ')' && stack_top == -1) {
                        break;
                    }
                    size_t qmk_len = strlen(special_key_mappings[i].qmk_expr);
                    if (output_pos + qmk_len < output_size) {
                        memcpy(&qmk_output[output_pos], special_key_mappings[i].qmk_expr, qmk_len);
                        output_pos += qmk_len;
                        input_pos += expr_len;
                        found_match = true;
                        break;
                    } else {
                        return ESP_ERR_NO_MEM;
                    }
                }
            }

            if (found_match) continue;

            // Check for modifier mappings
            for (int i = 0; modifier_mappings[i].user_expr != NULL; i++) {
                size_t expr_len = strlen(modifier_mappings[i].user_expr);
                if (strncmp(&user_input[input_pos], modifier_mappings[i].user_expr, expr_len) == 0) {
                    // Check stack overflow
                    if (stack_top >= MAX_MODIFIER_DEPTH - 1) {
                        ESP_LOGE(TAG, "Modifier nesting too deep at position %zu", input_pos);
                        return ESP_ERR_INVALID_ARG;
                    }

                    // Add modifier down
                    size_t down_len = strlen(modifier_mappings[i].qmk_expr);
                    if (output_pos + down_len >= output_size) {
                        return ESP_ERR_NO_MEM;
                    }
                    memcpy(&qmk_output[output_pos], modifier_mappings[i].qmk_expr, down_len);
                    output_pos += down_len;

                    // Push modifier to stack
                    stack_top++;
                    modifier_stack[stack_top] = i;

                    input_pos += expr_len;  // Skip past the opening parenthesis
                    found_match = true;
                    break;
                }
            }

            if (found_match) continue;
        }

        // Check for closing parenthesis
        if (user_input[input_pos] == ')') {
            if (stack_top >= 0) {
                // Pop modifier from stack and add UP command
                uint8_t modifier_idx = modifier_stack[stack_top];
                stack_top--;

                size_t up_len = strlen(modifier_up_mappings[modifier_idx].qmk_expr);
                if (output_pos + up_len >= output_size) {
                    return ESP_ERR_NO_MEM;
                }
                memcpy(&qmk_output[output_pos], modifier_up_mappings[modifier_idx].qmk_expr, up_len);
                output_pos += up_len;

                input_pos++;
                found_match = true;
            }
            // If stack_top is -1, treat ')' as a regular character (don't set found_match = true)
        }

        if (!found_match) {
            // Copy regular character
            if (output_pos + 1 >= output_size) {
                return ESP_ERR_NO_MEM;
            }
            // Range check for printable ASCII
            if (!is_printable_ascii(user_input[input_pos])) {
                ESP_LOGE(TAG, "Non-printable ASCII character at position %zu: 0x%02X", input_pos, (unsigned char)user_input[input_pos]);
                input_pos++;
                qmk_output[output_pos++] = '.';
            } else {
                qmk_output[output_pos++] = user_input[input_pos++];
            }
        }
    }

    if (input_pos < input_len && output_pos >= output_size - 1) {
        // No space for output
        return ESP_ERR_NO_MEM;
    }

    // Check for unmatched opening parentheses
    if (stack_top >= 0) {
        ESP_LOGE(TAG, "Unmatched opening parentheses");
        return ESP_ERR_INVALID_ARG;
    }

    qmk_output[output_pos] = '\0';
    return ESP_OK;
}

// Helper function to find mapping by QMK expression
static int find_mapping_by_qmk_expr(const expression_mapping_t* mappings, const char* qmk_input, size_t input_pos)
{
    for (int i = 0; mappings[i].user_expr != NULL; i++) {
        size_t qmk_len = strlen(mappings[i].qmk_expr);
        if (strncmp(&qmk_input[input_pos], mappings[i].qmk_expr, qmk_len) == 0) {
            return i;
        }
    }
    return -1;
}

// Helper function to emit user expression from mapping
static esp_err_t emit_user_expr(const expression_mapping_t* mappings, int mapping_idx,
                                char* user_output, size_t* output_pos, size_t output_size,
                                size_t* input_pos)
{
    size_t user_len = strlen(mappings[mapping_idx].user_expr);
    size_t qmk_len = strlen(mappings[mapping_idx].qmk_expr);

    if (*output_pos + user_len >= output_size) {
        return ESP_ERR_NO_MEM;
    }

    memcpy(&user_output[*output_pos], mappings[mapping_idx].user_expr, user_len);
    *output_pos += user_len;
    *input_pos += qmk_len;

    return ESP_OK;
}

// Convert QMK compatible representations back to user-friendly expressions
esp_err_t convert_qmk_to_user_friendly(const char* qmk_input, char* user_output, size_t output_size)
{
    if (!qmk_input || !user_output || output_size == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    size_t input_len = strlen(qmk_input);
    size_t output_pos = 0;
    size_t input_pos = 0;

    // Track modifier nesting depth without stack for performance
    int modifier_depth = 0;

    while (input_pos < input_len && output_pos < output_size - 1) {
        char current_char = qmk_input[input_pos];

        if (current_char == ')') {
            if (modifier_depth > 0) {
                // Output "\\)" for closing parenthesis inside modifier
                if (output_pos + 2 >= output_size) {
                    return ESP_ERR_NO_MEM;
                }
                user_output[output_pos++] = '\\';
                user_output[output_pos++] = ')';
            } else {
                // Output ")" for regular closing parenthesis
                if (output_pos + 1 >= output_size) {
                    return ESP_ERR_NO_MEM;
                }
                user_output[output_pos++] = ')';
            }
            input_pos++;
            continue;
        }

        // Check for QMK special codes (SS_TAP, SS_DOWN, SS_UP)
        if (input_pos + 2 < input_len && current_char == SS_QMK_PREFIX) {
            uint8_t code_type = qmk_input[input_pos + 1];

            switch (code_type) {
                case SS_TAP_CODE: {
                    // Handle SS_TAP codes for special keys
                    int mapping_idx = find_mapping_by_qmk_expr(special_key_mappings, qmk_input, input_pos);
                    if (mapping_idx >= 0) {
                        esp_err_t err = emit_user_expr(special_key_mappings, mapping_idx, user_output, &output_pos, output_size, &input_pos);
                        if (err != ESP_OK) return err;
                        continue;
                    }
                    break;
                }

                case SS_DOWN_CODE: {
                    // Emit modifier start when seeing SS_DOWN
                    int mapping_idx = find_mapping_by_qmk_expr(modifier_mappings, qmk_input, input_pos);
                    if (mapping_idx >= 0) {
                        esp_err_t err = emit_user_expr(modifier_mappings, mapping_idx, user_output, &output_pos, output_size, &input_pos);
                        if (err != ESP_OK) return err;

                        modifier_depth++;
                        continue;
                    }
                    break;
                }

                case SS_UP_CODE: {
                    // Fast path: Skip SS_UP code and next byte, output ")" directly
                    if (modifier_depth > 0) {
                        if (output_pos + 1 >= output_size) {
                            return ESP_ERR_NO_MEM;
                        }
                        user_output[output_pos++] = ')';
                        modifier_depth--;
                        input_pos += 3; // Skip SS_QMK_PREFIX, SS_UP_CODE, and keycode
                        continue;
                    } else {
                        ESP_LOGE(TAG, "Unmatched modifier UP code");
                        return ESP_ERR_INVALID_ARG;
                    }
                }
            }
        }

        // Copy regular character
        if (output_pos + 1 >= output_size) {
            return ESP_ERR_NO_MEM;
        }
        user_output[output_pos++] = qmk_input[input_pos++];
    }

    if (input_pos < input_len && output_pos >= output_size - 1) {
        // No space for output
        return ESP_ERR_NO_MEM;
    }

    // Check for unmatched modifiers
    if (modifier_depth > 0) {
        ESP_LOGE(TAG, "Unmatched modifier sequences in QMK input");
        return ESP_ERR_INVALID_ARG;
    }

    user_output[output_pos] = '\0';
    return ESP_OK;
}

// Enhanced set_macro_str function that converts user-friendly input to QMK format
esp_err_t set_macro_str(uint16_t keycode, char* buf)
{
    if (!buf) {
        return ESP_ERR_INVALID_ARG;
    }

    // Convert user-friendly input to QMK format
    char* qmk_buf = malloc(MACRO_STR_MAX_LEN);
    if (!qmk_buf) {
        return ESP_ERR_NO_MEM;
    }

    esp_err_t err = convert_user_friendly_to_qmk(buf, qmk_buf, MACRO_STR_MAX_LEN);
    if (err != ESP_OK) {
        free(qmk_buf);
        return err;
    }

    int blob_len = strlen(qmk_buf) + 1;

    if (blob_len > MACRO_STR_MAX_LEN) {
        ESP_LOGE(TAG, "macro string is too long, len=%d", blob_len);
        free(qmk_buf);
        return ESP_FAIL;
    }

    keycode -= MACRO_CODE_MIN;

    char scratch[8];
    int len = snprintf(scratch, sizeof(scratch), MACRO_NAME_PREFIX "%d", keycode);
    if (len >= sizeof(scratch) || len < 0) {
        ESP_LOGE(TAG, "keycode is too large, keycode=%u ", keycode);
        free(qmk_buf);
        return ESP_FAIL;
    }

    err = nvs_write_blob(MACRO_NAMESPACE, scratch, qmk_buf, blob_len);
    free(qmk_buf);
    return err;
}

// Original get_macro_str function (returns QMK format)
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

// New function to get macro in user-friendly format
esp_err_t get_macro_readable_str(uint16_t keycode, char* buf, int buf_len)
{
    if (!buf) {
        return ESP_ERR_INVALID_ARG;
    }

    // First get the QMK format
    char* qmk_buf = malloc(MACRO_STR_MAX_LEN);
    if (!qmk_buf) {
        return ESP_ERR_NO_MEM;
    }

    esp_err_t err = get_macro_str(keycode, qmk_buf, MACRO_STR_MAX_LEN);
    if (err != ESP_OK) {
        free(qmk_buf);
        return err;
    }

    // Convert QMK format to user-friendly format
    err = convert_qmk_to_user_friendly(qmk_buf, buf, buf_len);
    free(qmk_buf);

    return err;
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


