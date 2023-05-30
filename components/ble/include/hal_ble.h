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
 * Copyright 2019 Benjamin Aigner <beni@asterics-foundation.org>
 */

/** @file
 * @brief This file is a wrapper for the BLE-HID example of Espressif.
 */
#ifndef _HAL_BLE_H_
#define _HAL_BLE_H_

#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/queue.h>
#include <esp_log.h>

#include "esp_bt.h"
#include "esp_bt_defs.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_gatt_defs.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"

#define REPORT_LEN 8

/** @brief Stack size for BLE task */
#define TASK_BLE_STACKSIZE 2048

/** @brief Queue for sending keyboard reports
 * @see keyboard_command_t */
extern QueueHandle_t battery_q;

/** @brief Queue for sending keyboard reports
 * @see keyboard_command_t */
extern QueueHandle_t keyboard_q;

/** @brief Main init function to start HID interface (C interface)
 * @see hid_ble */
esp_err_t halBLEInit(const char* name);

bool isBLERunning();

#endif /* _HAL_BLE_H_ */
