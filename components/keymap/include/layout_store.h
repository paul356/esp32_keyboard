/*
 * Copyright (C) 2026 panhao356@gmail.com
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * layout_store.h
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

#ifndef NVS_FUNCS_H_
#define NVS_FUNCS_H_

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
 * @brief Get the current layout version
 *
 * @param version Pointer to store the version
 * @return esp_err_t ESP_OK on success, appropriate error code otherwise
 */
esp_err_t nvs_get_layout_version(uint64_t* version);

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

/**
 * @brief Update multiple layouts with a single version
 *
 * This function updates multiple layers in a single transaction and assigns a new version
 * to the entire keymap. It's useful for batch updates from the web interface.
 *
 * @param version New layout version to set (must be > 0)
 * @param nlayers Number of layers to update
 * @param layer_names Array of layer names to update
 * @param positions Array of position arrays, one per layer
 * @param keycodes Array of keycode arrays, one per layer
 * @param counts Array specifying number of changes for each layer
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t nvs_update_layout(uint64_t version, uint32_t nlayers,
                            const char** layer_names,
                            const uint16_t** positions,
                            const uint16_t** keycodes,
                            const uint16_t* nkeycode);

#endif /* NVS_FUNCS_H_ */
