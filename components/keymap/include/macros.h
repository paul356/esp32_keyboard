#pragma once

#include <stdbool.h>
#include "esp_err.h"
#include "quantum_keycodes.h"

#define MACRO_CODE_NUM 32
#define MACRO_CODE_MIN SAFE_RANGE
#define MACRO_CODE_MAX (MACRO_CODE_MIN + MACRO_CODE_NUM - 1)
#define MACRO_RESERVED_MAX (SAFE_RANGE + 256) // MACRO_CODE_NUM is not larger than 256

#define MACRO_STR_MAX_LEN 1024

esp_err_t set_macro_str(uint16_t keycode, char* buf);
esp_err_t get_macro_str(uint16_t keycode, char* buf, int buf_len);

esp_err_t get_macro_name(uint16_t keycode, char* buf, int buf_len);
esp_err_t parse_macro_name(const char* macro_name, uint16_t* keycode);

esp_err_t process_macro_code(uint16_t keycode);
