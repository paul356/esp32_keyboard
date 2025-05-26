/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * NVS IO utility functions for reading and writing data blobs to NVS
 */

#ifndef NVS_IO_H
#define NVS_IO_H

#include <stddef.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Read data blob from NVS
 * 
 * @param namespace The namespace to read from
 * @param key The key to read
 * @param buffer The buffer to read into
 * @param buf_size Pointer to variable containing buffer size
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t nvs_read_blob(const char* namespace, const char* key, void* buffer, size_t* buf_size);

/**
 * @brief Write data blob to NVS
 * 
 * @param namespace The namespace to write to
 * @param key The key to write
 * @param buffer The buffer to write from
 * @param buf_size Size of the buffer
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t nvs_write_blob(const char* namespace, const char* key, const void* buffer, size_t buf_size);

#ifdef __cplusplus
}
#endif

#endif /* NVS_IO_H */