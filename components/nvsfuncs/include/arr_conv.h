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
