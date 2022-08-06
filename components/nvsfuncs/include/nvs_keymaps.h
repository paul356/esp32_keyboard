/*
 * nvs_keymaps.h
 *
 *  Created on: 19 Sep 2018
 *      Author: gal
 */

#ifndef NVS_KEYMAPS_H_
#define NVS_KEYMAPS_H_
#include <keyboard_config.h>
#include "config.h"

//array for for holding the keymaps
extern uint16_t layouts[LAYERS][MATRIX_ROWS][MATRIX_COLS];

//array for keymap names
extern char **layer_names_arr;

extern uint16_t encoder_map[LAYERS][ENCODER_SIZE];

extern uint16_t slave_encoder_map[LAYERS][ENCODER_SIZE];

//amount of arrays
extern uint8_t layers_num;

#endif /* NVS_KEYMAPS_H_ */
