#ifndef KEY_DEFINITIONS_H
#define KEY_DEFINITIONS_H

#include <stdint.h>
#include "esp_err.h"

#define MAX_ARG_NUM 3

typedef enum {
    LAYER_NUM,
    BASIC_CODE,
    MOD_BITS,
    MACRO_CODE
} argument_type_e;

typedef struct {
    const char* desc;
    int num_args;
    argument_type_e arg_types[MAX_ARG_NUM];
} funct_desc_t;

const char* get_basic_key_name(uint16_t keycode);
uint16_t get_max_basic_key_code(void);

int get_total_funct_num(void);
esp_err_t get_funct_desc(int idx, funct_desc_t* desc);

int get_mod_bit_num(void);
const char* get_mod_bit_name(int idx);

int get_macro_num(void);
esp_err_t get_macro_name(int idx, char* buf, int buf_len);
esp_err_t parse_macro_name(const char* macro_name, uint16_t* keycode);

esp_err_t get_full_key_name(uint16_t keycode, char* buf, int buf_len);

esp_err_t parse_full_key_name(const char* full_name, uint16_t* keycode);

#endif
