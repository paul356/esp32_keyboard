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
#include "nvs.h"
#include "config.h"
#include "keyboard_config.h"

#ifndef NVS_FUNCS_H_
#define NVS_FUNCS_H_

#define NVS_CONFIG_OK 1
#define NVS_CONFIG_ERR 0

/**
 * @brief Read data blob from NVS
 */
esp_err_t nvs_read_blob(const char* namespace, const char* key, void* buffer, size_t* buf_size);

/**
 * @brief Write data blob to NVS
 */
esp_err_t nvs_write_blob(const char* namespace, const char* key, const void* buffer, size_t buf_size);

/**
 * @brief Reset layouts to default values
 */
esp_err_t nvs_reset_layouts(void);

/**
 * @brief Load layouts from NVS
 */
void nvs_load_layouts(void);

/**
 * @brief Get a layer from in-memory keymap
 */
esp_err_t nvs_get_layer(const char* layer_name, uint16_t* layout, uint16_t layout_len);

/**
 * @brief Save one layer to NVS. This function will update both in-memory keymap and nvs store.
 */
esp_err_t nvs_save_layer(const char* layer_name, const uint16_t* layout, uint16_t layout_len);

/**
 * @brief Get keymap information (layers, rows, cols)
 *
 * @param layers Pointer to store the number of layers
 * @param rows Pointer to store the number of rows
 * @param cols Pointer to store the number of columns
 * @return esp_err_t ESP_OK on success, appropriate error code otherwise
 */
esp_err_t nvs_get_keymap_info(uint8_t *layers, uint32_t *rows, uint32_t *cols);

/**
 * @brief Get the name of a layer
 *
 * @param layer Layer index
 * @return const char* Pointer to the layer name string
 */
const char* nvs_get_layer_name(uint8_t layer);

/**
 * @brief Get keycode at the specified position
 *
 * @param layer Layer index
 * @param row Row index
 * @param col Column index
 * @param keycode Pointer to store the keycode value
 * @return esp_err_t ESP_OK on success, appropriate error code otherwise
 */
esp_err_t nvs_get_keycode(uint8_t layer, uint8_t row, uint8_t col, uint16_t *keycode);

#endif /* NVS_FUNCS_H_ */
