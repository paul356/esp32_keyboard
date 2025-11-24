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

#ifndef __KEYMAP_SERVER_H__
#define __KEYMAP_SERVER_H__

#include "esp_err.h"

/**
 * @brief Start the HTTP file server
 *
 * Starts the HTTP server that serves the keymap configuration web interface
 * and handles API requests for keyboard configuration.
 *
 * @return
 *     - ESP_OK: Server started successfully
 *     - ESP_FAIL: Failed to start server
 *     - ESP_OK: Server already running (warning logged)
 */
esp_err_t start_file_server(void);

/**
 * @brief Stop the HTTP file server
 *
 * Gracefully stops the HTTP server if it is running.
 *
 * @return
 *     - ESP_OK: Server stopped successfully or was not running
 *     - Other ESP error codes: Failed to stop server
 */
esp_err_t stop_file_server(void);

#endif // __KEYMAP_SERVER_H__
