/*
 * nvs_funcs.h
 *
 *  Created on: 13 Sep 2018
 *      Author: gal
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "esp_err.h"
#include "config.h"
#include "keyboard_config.h"

#ifndef NVS_FUNCS_H_
#define NVS_FUNCS_H_

#define NVS_CONFIG_OK 1
#define NVS_CONFIG_ERR 0

// array to hold names of layouts for oled
extern char default_layout_names[LAYERS][MAX_LAYOUT_NAME_LENGTH];
extern const uint16_t _LAYERS[LAYERS][MATRIX_ROWS][MATRIX_COLS];

esp_err_t nvs_read_blob(const char* namespace, const char* key, void* buffer, size_t* buf_size);

esp_err_t nvs_write_blob(const char* namespace, const char* key, const void* buffer, size_t buf_size);

/*
 * @read a layout from nvs
 */
void nvs_read_layout(const char* layout_name, uint16_t buffer[MATRIX_ROWS*MATRIX_COLS]);

/*
 * @add a layout to nvs or overwrite existing one
 */
void nvs_write_layout(const char* layout_name, uint16_t layout[MATRIX_ROWS*MATRIX_COLS]);

/*
 * @brief read keyboard configuration from nvs
 */
void nvs_read_keymap_cfg(void);

/*
 * @brief write keyboard configuration to nvs (without keymaps)
 */
void nvs_write_keymap_cfg(uint8_t layers, char **layer_names);

esp_err_t nvs_reset_layouts(void);

/*
 * @load the layouts from nvs
 */
void nvs_load_layouts(void);

void nvs_save_layouts(void);

#endif /* NVS_FUNCS_H_ */
