/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * NVS IO utility functions for reading and writing data blobs to NVS
 */

#include <stdio.h>
#include <string.h>
#include "nvs_flash.h"
#include "esp_log.h"
#include "nvs_io.h"

static const char* TAG = "nvs_io";

/**
 * @brief Read data blob from NVS
 */
esp_err_t nvs_read_blob(const char* namespace, const char* key, void* buffer, size_t* buf_size)
{
    nvs_handle_t nvs_handle;
    esp_err_t err;

    // Open NVS handle
    err = nvs_open(namespace, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error (%s) opening NVS handle for %s", esp_err_to_name(err), namespace);
        return err;
    }

    // Read the blob
    err = nvs_get_blob(nvs_handle, key, buffer, buf_size);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGE(TAG, "Error (%s) reading blob %s", esp_err_to_name(err), key);
    }

    // Close NVS handle
    nvs_close(nvs_handle);
    return err;
}

/**
 * @brief Write data blob to NVS
 */
esp_err_t nvs_write_blob(const char* namespace, const char* key, const void* buffer, size_t buf_size)
{
    nvs_handle_t nvs_handle;
    esp_err_t err;

    // Open NVS handle
    err = nvs_open(namespace, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error (%s) opening NVS handle for %s", esp_err_to_name(err), namespace);
        return err;
    }

    // Write the blob
    err = nvs_set_blob(nvs_handle, key, buffer, buf_size);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error (%s) writing blob %s", esp_err_to_name(err), key);
        nvs_close(nvs_handle);
        return err;
    }

    // Commit changes
    err = nvs_commit(nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error (%s) committing changes to %s", esp_err_to_name(err), namespace);
    }

    // Close NVS handle
    nvs_close(nvs_handle);
    return err;
}