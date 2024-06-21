/*
 *  This program is a keyboard firmware for ESP family boards inspired by
 *  MK32 and qmk_firmware.
 *
 *  Copyright (C) 2024 github.com/paul356
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#ifndef ARR_CONV_H_
#define ARR_CONV_H_

#ifdef __cplusplus
extern "C" {
#endif

/*
 * @brief convert string array to single string, copy the data to the buffer
 */
void str_arr_to_str(char **layer_names, uint8_t layers, char **buffer);
/*
 * @brief convert string to string array, copy the data to the buffer
 */
void str_to_str_arr(char *str, uint8_t layers,char ***buffer);

#ifdef __cplusplus
}
#endif

#endif /* ARR_CONV_H_ */
