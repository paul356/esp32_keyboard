/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 *
 * Original work: Copyright 2018 Gal Zaidenstein.
 * Modified by panhao356@gmail.com under GPL-2.0-or-later
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
//#include "nvs_flash.h"
#include "nvs.h"
#include "nvs_io.h"
#include "layout_store.h"
#include "keymap.h"
#include "keymap_const.h"
#include <arr_conv.h>

#define NVS_TAG "NVS Storage"

#define KEYMAP_NAMESPACE "keymap_conf"

#define LAYOUT_NAMES "layouts"
#define LAYOUT_NUM "num_layouts"
#define LAYOUT_VERSION "layout_version"

#define MAX_LAYOUT_NAME_LENGTH 15
// Initial layout version
#define INITIAL_LAYOUT_VERSION 1ULL

// array to hold names of layouts for oled

// These are now static variables instead of external references
extern char default_layout_names[LAYERS][MAX_LAYOUT_NAME_LENGTH];
extern const uint16_t _LAYERS[LAYERS][MATRIX_ROWS][MATRIX_COLS];

static char **layer_names_arr;
static uint8_t layers_num=0;
static uint64_t current_layout_version = INITIAL_LAYOUT_VERSION;

uint16_t keymaps[LAYERS][MATRIX_ROWS][MATRIX_COLS];

static esp_err_t nvs_write_keymap_cfg(uint8_t layers, char **layer_names_arr);

//add or overwrite a keymap to the nvs - now static
static esp_err_t nvs_write_layout_matrix(const uint16_t layout[MATRIX_ROWS * MATRIX_COLS], const char* layout_name)
{
	esp_err_t err = nvs_write_blob(KEYMAP_NAMESPACE, layout_name, layout, sizeof(uint16_t) * MATRIX_ROWS * MATRIX_COLS);
	if (err != ESP_OK) {
		ESP_LOGE(NVS_TAG,"write ns:%s key:%s fail reason(%s)!\n", KEYMAP_NAMESPACE, layout_name, esp_err_to_name(err));
	}
	return err;
}

//read the what layouts are in the nvs - now static
static void nvs_read_keymap_cfg(void){
	ESP_LOGI(NVS_TAG,"Opening NVS handle");
	nvs_handle keymap_handle;
	esp_err_t err = nvs_open(KEYMAP_NAMESPACE, NVS_READWRITE, &keymap_handle);
	if (err != ESP_OK) {
		ESP_LOGE(NVS_TAG,"Error (%s) opening NVS handle!\n", esp_err_to_name(err));
		return;
	} else {
		ESP_LOGI(NVS_TAG,"NVS Handle opened successfully");
	}

	uint8_t layers=0;
	err = nvs_get_u8(keymap_handle, LAYOUT_NUM, &layers);
	if (err != ESP_OK) {
		ESP_LOGE(NVS_TAG, "Error getting layout num: %s", esp_err_to_name(err));
		nvs_close(keymap_handle);
		return;
	} else{
		ESP_LOGI(NVS_TAG, "Success getting layout num");
	}

	// get layout version
	err = nvs_get_u64(keymap_handle, LAYOUT_VERSION, &current_layout_version);
	if (err != ESP_OK) {
		ESP_LOGE(NVS_TAG, "Error getting layout version: %s", esp_err_to_name(err));
		nvs_close(keymap_handle);
		return;
	} else {
		ESP_LOGI(NVS_TAG, "Success getting layout version: %llu", (unsigned long long)current_layout_version);
	}

	size_t str_size = (MAX_LAYOUT_NAME_LENGTH + 1) * layers;
	char *layer_names = (char*)malloc(str_size);
	if (!layer_names) {
		ESP_LOGE(NVS_TAG, "no memory for layer name buffer");
		nvs_close(keymap_handle);
		return;
	}

	err = nvs_get_str(keymap_handle, LAYOUT_NAMES, layer_names, &str_size);
	if (err != ESP_OK) {
		ESP_LOGI(NVS_TAG, "Error getting layout names size: %s", esp_err_to_name(err));
		free(layer_names);
		nvs_close(keymap_handle);
		return;
	}

	if (layer_names_arr) {
		free_layer_names(&layer_names_arr, layers);
	}

	ESP_LOGI(NVS_TAG, "Success getting layout name");
	layers_num = layers;
	str_to_str_arr(layer_names, layers, &layer_names_arr);
	free(layer_names);

	for(uint8_t i = 0; i < layers; i++){
		ESP_LOGI(NVS_TAG, "\nLayer %d:  %s", i, layer_names_arr[i]);
	}
	nvs_close(keymap_handle);
}

//write layout names to nvs - now static
static esp_err_t nvs_write_keymap_cfg(uint8_t layers, char **layer_names_arr){

	char *layer_names;
	str_arr_to_str(layer_names_arr, layers, &layer_names);

	ESP_LOGI(NVS_TAG,"Opening NVS handle");
	nvs_handle keymap_handle;
	esp_err_t err = nvs_open(KEYMAP_NAMESPACE, NVS_READWRITE, &keymap_handle);
	if (err != ESP_OK) {
		ESP_LOGE(NVS_TAG,"Error (%s) opening NVS handle!\n", esp_err_to_name(err));
		free(layer_names);
		return err;
	} else {
		ESP_LOGI(NVS_TAG,"NVS Handle opened successful");
	}

	err = nvs_set_u8(keymap_handle, LAYOUT_NUM, layers);
	if (err != ESP_OK) {
		ESP_LOGE(NVS_TAG, "Error setting layout num: %s", esp_err_to_name(err));
		goto cleanup;
	} else{
		ESP_LOGI(NVS_TAG, "Success setting layout num");
	}

	err = nvs_set_str(keymap_handle, LAYOUT_NAMES, layer_names);
	if (err != ESP_OK) {
		ESP_LOGE(NVS_TAG, "Error setting layout names: %s", esp_err_to_name(err));
		goto cleanup;
	} else{
		ESP_LOGI(NVS_TAG, "Success setting layout names");
	}

	err = nvs_set_u64(keymap_handle, LAYOUT_VERSION, current_layout_version);
	if (err != ESP_OK) {
		ESP_LOGE(NVS_TAG, "Error setting layout version: %s", esp_err_to_name(err));
		goto cleanup;
	}

	err = nvs_commit(keymap_handle);
	if (err != ESP_OK) {
		ESP_LOGE(NVS_TAG, "Error committing changes: %s", esp_err_to_name(err));
	}

cleanup:
	nvs_close(keymap_handle);
	free(layer_names);

	return err;
}

esp_err_t nvs_get_layer(const char* layer_name, uint16_t* layout, uint16_t len)
{
	ESP_LOGI(NVS_TAG, "Loading layout for layer %s", layer_name);
	if (len != MATRIX_ROWS * MATRIX_COLS) {
		ESP_LOGE(NVS_TAG, "Invalid layout length");
		return ESP_ERR_INVALID_ARG;
	}

	// check if layout exits
	for (uint8_t i = 0; i < layers_num; i++)
	{
		if (strcmp(layer_name, layer_names_arr[i]) == 0)
		{
			memcpy(layout, &keymaps[i][0][0], sizeof(uint16_t) * MATRIX_ROWS * MATRIX_COLS);
			return ESP_OK;
		}
	}

	ESP_LOGE(NVS_TAG, "Layer %s not found", layer_name);
	return ESP_ERR_INVALID_ARG;
}

static esp_err_t nvs_save_layouts(void)
{
	ESP_LOGI(NVS_TAG, "Saving layouts to NVS");
	esp_err_t err = nvs_write_keymap_cfg(layers_num, layer_names_arr);
	if (err != ESP_OK) {
		ESP_LOGE(NVS_TAG, "Error writing keymap config: %s", esp_err_to_name(err));
		return err;
	}

	for (uint8_t i = 0; i < layers_num; i++) {
		err = nvs_write_layout_matrix(&keymaps[i][0][0], layer_names_arr[i]);
		if (err != ESP_OK) {
			ESP_LOGE(NVS_TAG, "Error writing layout %s: %s", layer_names_arr[i], esp_err_to_name(err));
			return err;
		}
	}
	return ESP_OK;
}

esp_err_t nvs_reset_layouts(void)
{
	free_layer_names(&layer_names_arr, layers_num);
	layer_names_arr = malloc(sizeof(void*) * LAYERS);
	if (!layer_names_arr) {
		return ESP_ERR_NO_MEM;
	}
	for(uint8_t i = 0;i < LAYERS; i++){
		layer_names_arr[i] = malloc(sizeof(default_layout_names[i]));
		if (!layer_names_arr[i]) {
			free_layer_names(&layer_names_arr, i);
			return ESP_ERR_NO_MEM;
		}
		strcpy(layer_names_arr[i], default_layout_names[i]);
	}
	memcpy(&keymaps[0][0][0], &_LAYERS[0][0][0], sizeof(_LAYERS));

	// layout is copy from default, save these configs
	layers_num = LAYERS;
	current_layout_version = INITIAL_LAYOUT_VERSION;
	return nvs_save_layouts();
}

//load the layouts from nvs
void nvs_load_layouts(void)
{
	ESP_LOGI(NVS_TAG,"Loading layouts");
	nvs_read_keymap_cfg();

	if(layers_num != 0){
		// set keymap layouts
		ESP_LOGI(NVS_TAG,"Layouts found on NVS, loading layouts");
		for(uint8_t i = 0; i < layers_num; i++){
			size_t arr_size = sizeof(uint16_t) * MATRIX_ROWS * MATRIX_COLS;
			nvs_read_blob(KEYMAP_NAMESPACE, layer_names_arr[i], &keymaps[i][0][0], &arr_size);
		}
	} else {
		ESP_LOGI(NVS_TAG,"Layouts not found on NVS, loading default layouts");
		(void)nvs_reset_layouts();
	}
}

esp_err_t nvs_get_keymap_info(uint8_t *layers, uint32_t *rows, uint32_t *cols) {
	if (!layers || !rows || !cols) {
		return ESP_ERR_INVALID_ARG;
	}

	*layers = layers_num; // Number of layers
	*rows = MATRIX_ROWS;
	*cols = MATRIX_COLS;

	return ESP_OK;
}

const char* nvs_get_layer_name(uint8_t layer) {
	if (layer >= layers_num) {
		return NULL;
	}
	return layer_names_arr[layer];
}

esp_err_t nvs_get_keycode(uint8_t layer, uint8_t row, uint8_t col, uint16_t *keycode) {
	if (!keycode) {
		return ESP_ERR_INVALID_ARG;
	}

	// Bounds checking
	if (layer >= layers_num || row >= MATRIX_ROWS || col >= MATRIX_COLS) {
		return ESP_ERR_INVALID_ARG;
	}

	// Get the keycode
	*keycode = keymaps[layer][row][col];

	return ESP_OK;
}

/**
 * @brief Get the current layout version
 */
esp_err_t nvs_get_layout_version(uint64_t* version)
{
	if (!version) {
		return ESP_ERR_INVALID_ARG;
	}

    *version = current_layout_version;
    return ESP_OK;
}

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
							const uint16_t* counts)
{
	esp_err_t err = ESP_OK;
	bool update_needed = false;

	// Basic validation
	if (version == 0 || nlayers == 0 || !layer_names || !positions || !keycodes || !counts) {
		ESP_LOGW(NVS_TAG, "Invalid parameters for nvs_update_layout");
		return ESP_ERR_INVALID_ARG;
	}

	ESP_LOGI(NVS_TAG, "Updating %" PRIu32 " layers with version %" PRIu64, nlayers, version);

	if (version != current_layout_version + 1) {
		ESP_LOGW(NVS_TAG, "Version must be exactly one greater than the current version");
		return ESP_ERR_INVALID_VERSION;
	}

	nvs_handle_t nvs_handle;
	err = nvs_open(KEYMAP_NAMESPACE, NVS_READWRITE, &nvs_handle);
	if (err != ESP_OK) {
		ESP_LOGE(NVS_TAG, "Error opening NVS namespace: %s", esp_err_to_name(err));
		return err;
	}

	// Process each layer
	for (uint32_t i = 0; i < nlayers; i++) {
		const char* layer_name = layer_names[i];
		uint16_t change_count = counts[i];

		// Skip empty layers
		if (change_count == 0 || !layer_name) {
			ESP_LOGD(NVS_TAG, "Skipping layer %d - no changes or no name", (int)i);
			continue;
		}

		// Find the layer index for this name
		int8_t layer_idx = -1;
		for (uint8_t j = 0; j < layers_num; j++) {
			if (strcmp(layer_name, layer_names_arr[j]) == 0) {
				layer_idx = j;
				break;
			}
		}

		// Validate layer index
		if (layer_idx < 0) {
			ESP_LOGE(NVS_TAG, "Layer name '%s' not found in stored layouts", layer_name);
			err = ESP_ERR_INVALID_ARG;
			break;
		}

		ESP_LOGI(NVS_TAG, "Updating layer '%s' (index %d) with %d changes",
				 layer_name, (int)layer_idx, (int)change_count);

		// Create a copy of the layer
		uint16_t layout[MATRIX_ROWS * MATRIX_COLS];
		memcpy(layout, &keymaps[layer_idx][0][0], sizeof(uint16_t) * MATRIX_ROWS * MATRIX_COLS);

		// Apply all changes to the layer
		for (uint16_t j = 0; j < change_count; j++) {
			uint16_t pos = positions[i][j];
			uint16_t kc = keycodes[i][j];

			// Bounds check
			if (pos >= MATRIX_ROWS * MATRIX_COLS) {
				ESP_LOGW(NVS_TAG, "Position %d out of range for layer '%s'", (int)pos, layer_name);
				continue;
			}

			// Update the keycode
			layout[pos] = kc;
			update_needed = true;
			ESP_LOGD(NVS_TAG, "Layer '%s': position %d updated with keycode 0x%04x",
					 layer_name, (int)pos, kc);
		}

		// Save the updated layer to NVS
		if (update_needed) {
			err = nvs_set_blob(nvs_handle, layer_name, layout, sizeof(uint16_t) * MATRIX_ROWS * MATRIX_COLS);
			if (err != ESP_OK) {
				ESP_LOGE(NVS_TAG, "Failed to save layer '%s': %s", layer_name, esp_err_to_name(err));
				break;
			}

			// Update in-memory keymaps
			memcpy(&keymaps[layer_idx][0][0], layout, sizeof(uint16_t) * MATRIX_ROWS * MATRIX_COLS);
		}
	}

	// Only update the version if changes were actually made
	if (update_needed && err == ESP_OK) {
		current_layout_version = version;
		err = nvs_set_u64(nvs_handle, LAYOUT_VERSION, version);
		if (err != ESP_OK) {
			ESP_LOGE(NVS_TAG, "Failed to update layout version: %s", esp_err_to_name(err));
			goto close_handle;
		}
		ESP_LOGI(NVS_TAG, "Layout version updated to %" PRIu64, version);

		err = nvs_commit(nvs_handle);
		if (err != ESP_OK) {
			ESP_LOGE(NVS_TAG, "Failed to commit changes: %s", esp_err_to_name(err));
			goto close_handle;
		}
	}

close_handle:
	(void)nvs_close(nvs_handle);

	return err;
}