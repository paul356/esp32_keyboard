#pragma once

// function keys follows macros
#include "macros.h"

#define FUNCTION_KEY_MIN MACRO_RESERVED_MAX
#define FUNCTION_KEY_MAX (FUNCTION_KEY_BUTT - 1)
#define FUNCTION_KEY_NUM (FUNCTION_KEY_BUTT - FUNCTION_KEY_MIN)

enum {
    FUNCTION_KEY_DEVICE_INFO = FUNCTION_KEY_MIN,
    FUNCTION_KEY_INTRO,
    FUNCTION_KEY_BUTT
};

const char *get_function_key_str(uint16_t keycode);
esp_err_t parse_function_key_str(const char* str, uint16_t *keycode);
esp_err_t process_function_key(uint16_t keycode);

