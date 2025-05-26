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
 * Modified by github.com/paul356 under GPL-2.0-or-later
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "nvs_io.h"
#include "layout_store.h"
#include "keymap.h"
#include <arr_conv.h>

#define NVS_TAG "NVS Storage"

#define KEYMAP_NAMESPACE "keymap_conf"

#define LAYOUT_NAMES "layouts"
#define LAYOUT_NUM "num_layouts"

#define MAX_LAYOUT_NAME_LENGTH 15
// array to hold names of layouts for oled

// These are now static variables instead of external references
extern char default_layout_names[LAYERS][MAX_LAYOUT_NAME_LENGTH];
extern const uint16_t _LAYERS[LAYERS][MATRIX_ROWS][MATRIX_COLS];

static char **layer_names_arr;
static uint8_t layers_num=0;

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
	err =nvs_get_u8(keymap_handle, LAYOUT_NUM, &layers);
	if (err != ESP_OK) {
		ESP_LOGE(NVS_TAG, "Error getting layout num: %s", esp_err_to_name(err));
        return;
	} else{
		ESP_LOGI(NVS_TAG, "Success getting layout num");
	}

    size_t str_size = (MAX_LAYOUT_NAME_LENGTH + 1) * layers;
    char *layer_names = (char*)malloc(str_size);
    if (!layer_names) {
        ESP_LOGE(NVS_TAG, "no memory for layer name buffer");
        return;
    }

	//set string array size
	//get string array size
	err = nvs_get_str(keymap_handle, LAYOUT_NAMES, layer_names, &str_size);
	if (err != ESP_OK) {
		ESP_LOGI(NVS_TAG, "Error getting layout names size: %s", esp_err_to_name(err));
        free(layer_names);
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
        free(layer_names);
        return err;
	} else{
		ESP_LOGI(NVS_TAG, "Success setting layout num");
	}

	err = nvs_set_str(keymap_handle, LAYOUT_NAMES, layer_names);
	if (err != ESP_OK) {
		ESP_LOGE(NVS_TAG, "Error setting layout names: %s", esp_err_to_name(err));
        free(layer_names);
        return err;
    } else{
		ESP_LOGI(NVS_TAG, "Success setting layout names");
	}

	err = nvs_commit(keymap_handle);
    if (err != ESP_OK) {
        ESP_LOGE(NVS_TAG, "Error committing changes: %s", esp_err_to_name(err));
    }

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

esp_err_t nvs_save_layer(const char* layer_name, const uint16_t* layout, uint16_t len)
{
    ESP_LOGI(NVS_TAG, "Saving layout for layer %s", layer_name);
    if (len != MATRIX_ROWS * MATRIX_COLS) {
        ESP_LOGE(NVS_TAG, "Invalid layout length");
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t layer = layers_num;
    // check if layout exits
    for (uint8_t i = 0; i < layers_num; i++)
    {
        if (strcmp(layer_name, layer_names_arr[i]) == 0)
        {
            layer = i;
            break;
        }
    }

    if (layer == layers_num)
    {
        ESP_LOGE(NVS_TAG, "Layer %s not found", layer_name);
        return ESP_ERR_INVALID_ARG;
    }

    memcpy(&keymaps[layer][0][0], layout, sizeof(uint16_t) * MATRIX_ROWS * MATRIX_COLS);
    return nvs_write_layout_matrix(&keymaps[layer][0][0], layer_names_arr[layer]);
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
    *rows = MATRIX_ROWS;  // From keyboard_config.h
    *cols = MATRIX_COLS;  // From keyboard_config.h

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
