/*
 * SPDX-FileCopyrightText: 2021-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#ifndef _BLE_GAP_H_
#define _BLE_GAP_H_

#define HIDD_IDLE_MODE 0x00
#define HIDD_BLE_MODE 0x01
#define HIDD_BT_MODE 0x02
#define HIDD_BTDM_MODE 0x03

#include "esp_err.h"
#include "esp_log.h"

#include "esp_bt.h"
#include "esp_bt_defs.h"
#include "esp_bt_main.h"
#include "esp_gap_bt_api.h"
#include "esp_hid_common.h"
#include "esp_gattc_api.h"
#include "esp_gatt_defs.h"
#include "esp_gap_ble_api.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t ble_gap_init(uint8_t mode);
esp_err_t ble_gap_adv_init(uint16_t appearance, const char *device_name);
esp_err_t ble_gap_adv_start(void);

#ifdef __cplusplus
}
#endif

#endif /* _BLE_GAP_H_ */