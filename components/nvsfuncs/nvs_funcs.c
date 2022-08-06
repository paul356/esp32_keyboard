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
 * Copyright 2018 Gal Zaidenstein.
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
#include "nvs_funcs.h"
#include "keymap.h"
#include <arr_conv.h>

#define NVS_TAG "NVS Storage"

#define KEYMAP_NAMESPACE "keymap_conf"

#define LAYOUT_NAMES "layouts"
#define LAYOUT_NUM "num_layouts"

char **layer_names_arr;
uint8_t layers_num=0;

uint16_t keymaps[LAYERS][MATRIX_ROWS][MATRIX_COLS];

static esp_err_t nvs_read_blob(const char* namespace, const char* key, void* buffer, size_t* buf_size)
{
	ESP_LOGI(NVS_TAG,"Opening NVS handle for %s", namespace);
    nvs_handle handle;
	esp_err_t err = nvs_open(namespace, NVS_READWRITE, &handle);
	if (err != ESP_OK) {
		ESP_LOGE(NVS_TAG,"Error (%s) opening NVS handle!\n", esp_err_to_name(err));
        return err;
	}

    ESP_LOGI(NVS_TAG,"NVS Handle for %s opened successfully", namespace);
    
	//get blob array size
	err = nvs_get_blob(handle, key, buffer, buf_size);
	if (err != ESP_OK) {
		ESP_LOGE(NVS_TAG, "Error getting layout: %s", esp_err_to_name(err));
        return err;
	}

	ESP_LOGI(NVS_TAG, "ns:%s key:%s copied to buffer", namespace, key);
	nvs_close(handle);
    return ESP_OK;
}

//read a layout from nvs
void nvs_read_layout(const char* layout_name, uint16_t buffer[MATRIX_ROWS*MATRIX_COLS])
{
    size_t arr_size = sizeof(uint16_t) * MATRIX_ROWS * MATRIX_COLS;
    esp_err_t err = nvs_read_blob(KEYMAP_NAMESPACE, layout_name, buffer, &arr_size);
    if (err != ESP_OK) {
        ESP_LOGE(NVS_TAG, "read ns:%s key:%s failed", KEYMAP_NAMESPACE, layout_name);
    }
}

static esp_err_t nvs_write_blob(const char* namespace, const char* key, void* buffer, size_t buf_size)
{
	ESP_LOGI(NVS_TAG,"Opening NVS handle");
    nvs_handle handle;
	esp_err_t err = nvs_open(namespace, NVS_READWRITE, &handle);
	if (err != ESP_OK) {
		ESP_LOGE(NVS_TAG,"Error (%s) opening NVS handle for %s!\n", esp_err_to_name(err), namespace);
        return err;
    }

    ESP_LOGI(NVS_TAG,"NVS Handle opened successfully");

    err = nvs_set_blob(handle, key, buffer, buf_size);
	if (err != ESP_OK) {
		ESP_LOGE(NVS_TAG, "Error writing ns:%s key:%s layout: %s", namespace, key, esp_err_to_name(err));
        return err;
	}

    ESP_LOGI(NVS_TAG, "Success writing layout");

    nvs_commit(handle);
	nvs_close(handle);
    return ESP_OK;
}

//add or overwrite a keymap to the nvs
static void nvs_write_layout_matrix(uint16_t layout[MATRIX_ROWS * MATRIX_COLS], const char* layout_name){
    esp_err_t err = nvs_write_blob(KEYMAP_NAMESPACE, layout_name, layout, sizeof(uint16_t) * MATRIX_ROWS * MATRIX_COLS);
    if (err != ESP_OK) {
		ESP_LOGE(NVS_TAG,"write ns:%s key:%s fail reason(%s)!\n", KEYMAP_NAMESPACE, layout_name, esp_err_to_name(err));
    }
}

//add or overwrite a keymap to the nvs, also take care of layers_num, layer_names_arr
void nvs_write_layout(uint16_t layout[MATRIX_ROWS * MATRIX_COLS], const char* layout_name){

	ESP_LOGI(NVS_TAG,"Adding/Modifying Layout");
	uint8_t FOUND_MATCH = 0;
	uint8_t cur_layers_num = layers_num;

	//check if layout exits
	for(uint8_t i=0;i<layers_num;i++){
		if(strcmp(layout_name, layer_names_arr[i])==0){
			FOUND_MATCH =1;
			break;
		}
	}

	//if layout exists
	if(FOUND_MATCH==1){
		ESP_LOGI(NVS_TAG,"Layout name previously listed: %s",layout_name);
	} else { //if layout doesn't exist
		cur_layers_num++;
        if (cur_layers_num > LAYERS) {
            ESP_LOGE(NVS_TAG, "max layer number(%d) reached", cur_layers_num);
            return;
        }
        
		ESP_LOGI(NVS_TAG,"New layout: %s",layout_name);
		char **curr_array;
		curr_array = (char **)malloc(sizeof(char*) * cur_layers_num);
        // copy current layer name address
        for(uint8_t i=0; i<layers_num; i++){
			curr_array[i] = layer_names_arr[i];
		}
        curr_array[cur_layers_num - 1] = strdup(layout_name);
		nvs_write_keymap_cfg(cur_layers_num, curr_array);

        // swap new layer names
        free(layer_names_arr);
        layer_names_arr = curr_array;

        layers_num = cur_layers_num;
        memcpy(&keymaps[cur_layers_num - 1][0][0], layout, sizeof(uint16_t) * MATRIX_ROWS * MATRIX_COLS);
	}
    
	nvs_write_layout_matrix(layout, layout_name);
}

void free_layer_names(char*** layer_names, uint32_t layers)
{
    for (uint32_t i = 0; i < layers; i++) {
        if (*layer_names) {
            free((*layer_names)[i]);
        }
    }
    free(*layer_names);
    *layer_names = NULL;
}

//read the what layouts are in the nvs
void nvs_read_keymap_cfg(void){
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

    uint32_t str_size = (MAX_LAYOUT_NAME_LENGTH + 1) * layers;
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

//write layout names to nvs
void nvs_write_keymap_cfg(uint8_t layers, char **layer_names_arr){

	char *layer_names;
	str_arr_to_str(layer_names_arr,layers,&layer_names);

	ESP_LOGI(NVS_TAG,"Opening NVS handle");
    nvs_handle keymap_handle;
	esp_err_t err = nvs_open(KEYMAP_NAMESPACE, NVS_READWRITE, &keymap_handle);
	if (err != ESP_OK) {
		ESP_LOGE(NVS_TAG,"Error (%s) opening NVS handle!\n", esp_err_to_name(err));
        free(layer_names);
        return;
	} else {
		ESP_LOGI(NVS_TAG,"NVS Handle opened successful");
	}

	err = nvs_set_u8(keymap_handle, LAYOUT_NUM, layers);
	if (err != ESP_OK) {
		ESP_LOGE(NVS_TAG, "Error setting layout num: %s", esp_err_to_name(err));
        free(layer_names);
        return;
	} else{
		ESP_LOGI(NVS_TAG, "Success setting layout num");
	}

	err = nvs_set_str(keymap_handle, LAYOUT_NAMES, layer_names);
	if (err != ESP_OK) {
		ESP_LOGE(NVS_TAG, "Error setting layout names: %s", esp_err_to_name(err));
        free(layer_names);
        return;
    } else{
		ESP_LOGI(NVS_TAG, "Success setting layout names");
	}
    
	nvs_commit(keymap_handle);
	nvs_close(keymap_handle);
	free(layer_names);
}

void nvs_store_layouts(void)
{
    ESP_LOGI(NVS_TAG, "Storing layouts");

    nvs_write_keymap_cfg(layers_num, layer_names_arr);

    for (uint16_t i = 0; i < layers_num; i++) {
        nvs_write_layout_matrix(&keymaps[i][0][0], layer_names_arr[i]);
    }
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
			nvs_read_layout(layer_names_arr[i], &keymaps[i][0][0]);
		}
	} else {
		ESP_LOGI(NVS_TAG,"Layouts not found on NVS, loading default layouts");
		free_layer_names(&layer_names_arr, LAYERS);
		layer_names_arr = malloc(sizeof(default_layout_names));
		for(uint8_t i = 0;i < LAYERS; i++){
			layer_names_arr[i] = malloc(sizeof(default_layout_names[i]));
			strcpy(layer_names_arr[i],default_layout_names[i]);
		}
        memcpy(&keymaps[0][0][0], &_LAYERS[0][0][0], sizeof(_LAYERS));
	}

    if (layers_num == 0) {
        // layout is copy from default, save these configs
        layers_num = LAYERS;
        nvs_store_layouts();
    }
}
