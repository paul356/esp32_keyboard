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
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include <arr_conv.h>
#include "nvs_funcs.h"

void free_layer_names(char*** layer_names, uint32_t layers);

//convert string array to a single string
void str_arr_to_str(char **layer_names, uint8_t layers, char **buffer){

	char str[(layers+(layers*MAX_LAYOUT_NAME_LENGTH))*sizeof(char)];
	memset( str, 0, (layers+(layers*MAX_LAYOUT_NAME_LENGTH))*sizeof(char) );
	for (uint8_t i=0;i<layers;i++){
		strcat(str,layer_names[i]);
		strcat(str,",");
	}
	*buffer = malloc(sizeof(str));
	strcpy(*buffer,str);
}

//convert string to string array
void str_to_str_arr(char *str, uint8_t layers, char ***buffer){
    *buffer = (char**)malloc(layers * sizeof(char*));
    if (*buffer) {
        memset(*buffer, 0, layers * sizeof(char*));
    } else {
        free_layer_names(buffer, layers);
        return;
    }

	const char* token = ",";
    char *next;
    uint32_t i = 0;
    for (char* name = strtok_r(str, token, &next);
         name && i < layers;
         name = strtok_r(NULL, token, &next))
    {
        (*buffer)[i] = (char*)malloc(strlen(name) + 1);
        if (!((*buffer)[i])) {
            free_layer_names(buffer, layers);
            return;
        } else {
            strcpy((*buffer)[i], name);
        }
        i++;
    }
}
