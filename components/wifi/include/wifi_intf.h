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

#ifndef __WIFI_AP_H__
#define __WIFI_AP_H__
#include "esp_wifi.h"

esp_err_t wifi_update(wifi_mode_t mode, const char* ssid, const char* passwd);
esp_err_t wifi_init(wifi_mode_t mode, const char* ssid, const char* passwd);
esp_err_t wifi_stop(void);

esp_err_t get_ip_addr(char* buf, int buf_size);

#endif
