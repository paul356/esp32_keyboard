#pragma once

#include <stdbool.h>
#include "esp_err.h"
#include "quantum_keycodes.h"

#define MACRO_CODE_NUM 32
#define MACRO_CODE_MIN SAFE_RANGE
#define MACRO_CODE_MAX (MACRO_CODE_MIN + MACRO_CODE_NUM - 1)

#define MACRO_NAME_PREFIX "MACRO"
#define MACRO_STR_MAX_LEN 1024

esp_err_t set_macro_str(uint16_t keycode, char* buf);
esp_err_t get_macro_str(uint16_t keycode, char* buf, int buf_len);
