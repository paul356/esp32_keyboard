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

#ifndef __HID_H__
#define __HID_H__

#include "report.h"

#define ESP32S3_EPSIZE 16 // MAX interrupt packet size for esp32s3
#define BOOT_REPORT_LEN 8
#define BOOT_REPORT_OFFSET 2

#ifdef NKRO_ENABLE
#define MAX_REPORT_LEN (KEYBOARD_REPORT_BITS + 1)
#else
#define MAX_REPORT_LEN BOOT_REPORT_LEN
#endif

enum {
    ITF_NUM_BOOT_KB = 0,
    ITF_NUM_COMPOSITE,
    ITF_NUM_TOTAL
};

#define HID_REPORT_DESC_NUM 3

void enable_usb_hid(void);

int get_hid_report_desc(const uint8_t** report_start, size_t* report_len, int arr_len);

bool is_caps_on(void);

void set_caps_state(bool state);

bool is_boot_protocol(void);

void set_boot_protocol(bool boot);

#endif
