/*
 *  This program is a keyboard firmware for ESP family boards inspired by
 *  MK32 and qmk_firmware.
 *
 *  Copyright (C) 2024 panhao356@gmail.com
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

#include "tinyusb.h"
#include "hid_desc.h"
#include "report.h"
#include "esp_log.h"
#include "keycode_config.h"

#define TAG "HID_DESC"
#define EPNUM_HID1 0x3
#define EPNUM_HID2 0x4

enum {
    STRID_LANGID = 0,
    STRID_MANUFACTURER,
    STRID_PRODUCT,
    STRID_SERIAL,
    STRID_HID
};

enum {
    TUSB_DESC_TOTAL_LEN = TUD_CONFIG_DESC_LEN + TUD_HID_DESC_LEN * 2
};

#ifdef NKRO_ENABLE
#define TUD_HID_REPORT_DESC_NKRO_KEYBOARD(...) \
  HID_USAGE_PAGE ( HID_USAGE_PAGE_DESKTOP     )                    ,\
  HID_USAGE      ( HID_USAGE_DESKTOP_KEYBOARD )                    ,\
  HID_COLLECTION ( HID_COLLECTION_APPLICATION )                    ,\
    /* Report ID if any */\
    __VA_ARGS__ \
    /* 8 bits Modifier Keys (Shift, Control, Alt) */ \
    HID_USAGE_PAGE ( HID_USAGE_PAGE_KEYBOARD )                     ,\
      HID_USAGE_MIN    ( 224                                    )  ,\
      HID_USAGE_MAX    ( 231                                    )  ,\
      HID_LOGICAL_MIN  ( 0                                      )  ,\
      HID_LOGICAL_MAX  ( 1                                      )  ,\
      HID_REPORT_COUNT ( 8                                      )  ,\
      HID_REPORT_SIZE  ( 1                                      )  ,\
      HID_INPUT        ( HID_DATA | HID_VARIABLE | HID_ABSOLUTE )  ,\
    /* 6-byte Keycodes */ \
    HID_USAGE_PAGE ( HID_USAGE_PAGE_KEYBOARD )                     ,\
      HID_USAGE_MIN    ( 0                                   )     ,\
      HID_USAGE_MAX    ( (KEYBOARD_REPORT_BITS * 8 - 1)      )     ,\
      HID_LOGICAL_MIN  ( 0                                   )     ,\
      HID_LOGICAL_MAX  ( 1                                   )     ,\
      HID_REPORT_COUNT ( (KEYBOARD_REPORT_BITS * 8)          )     ,\
      HID_REPORT_SIZE  ( 1                                   )     ,\
      HID_INPUT        ( HID_DATA | HID_VARIABLE | HID_ABSOLUTE )     ,\
    /* Output 5-bit LED Indicator Kana | Compose | ScrollLock | CapsLock | NumLock */ \
    HID_USAGE_PAGE  ( HID_USAGE_PAGE_LED                   )       ,\
      HID_USAGE_MIN    ( 1                                       ) ,\
      HID_USAGE_MAX    ( 5                                       ) ,\
      HID_REPORT_COUNT ( 5                                       ) ,\
      HID_REPORT_SIZE  ( 1                                       ) ,\
      HID_OUTPUT       ( HID_DATA | HID_VARIABLE | HID_ABSOLUTE  ) ,\
      /* led padding */ \
      HID_REPORT_COUNT ( 1                                       ) ,\
      HID_REPORT_SIZE  ( 3                                       ) ,\
      HID_OUTPUT       ( HID_CONSTANT                            ) ,\
  HID_COLLECTION_END \

  #endif

// Helper macros to calculate descriptor sizes
#define BOOT_KEYBOARD_DESC_SIZE sizeof((uint8_t[]){TUD_HID_REPORT_DESC_KEYBOARD(HID_REPORT_ID(REPORT_ID_KEYBOARD))})
#ifdef NKRO_ENABLE
#define NKRO_KEYBOARD_DESC_SIZE sizeof((uint8_t[]){TUD_HID_REPORT_DESC_NKRO_KEYBOARD(HID_REPORT_ID(REPORT_ID_NKRO))})
#endif

uint8_t const desc_hid_report[] = {
    TUD_HID_REPORT_DESC_KEYBOARD(HID_REPORT_ID(REPORT_ID_KEYBOARD)),
#ifdef NKRO_ENABLE
    TUD_HID_REPORT_DESC_NKRO_KEYBOARD(HID_REPORT_ID(REPORT_ID_NKRO))
#endif
};

// For boot protocol mode the report map should not contain report id.
// Below we should use 0 zero for the report.
uint8_t const desc_boot_report[] ={
    TUD_HID_REPORT_DESC_KEYBOARD()
};

static uint8_t const descriptor_hid_default[] = {
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, TUSB_DESC_TOTAL_LEN, TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 500),
    TUD_HID_DESCRIPTOR(ITF_NUM_BOOT_KB, STRID_HID, 1, sizeof(desc_boot_report), 0x80 | EPNUM_HID1, ESP32S3_EPSIZE, 10),
    TUD_HID_DESCRIPTOR(ITF_NUM_COMPOSITE, STRID_HID, 0, sizeof(desc_hid_report), 0x80 | EPNUM_HID2, ESP32S3_EPSIZE, 10)
};

static const char * descriptor_str_default[] = {
    // array of pointer to string descriptors
    (char[]){0x09, 0x04},                // 0: is supported language is English (0x0409)
    CONFIG_TINYUSB_DESC_MANUFACTURER_STRING, // 1: Manufacturer
    CONFIG_TINYUSB_DESC_PRODUCT_STRING,      // 2: Product
    CONFIG_TINYUSB_DESC_SERIAL_STRING,       // 3: Serials, should use chip ID
    "esp32 keyboard",
    NULL
};

static bool caps_on;
// define var keyboard_protocol for tmk_core
uint8_t keyboard_protocol = 1;

bool is_caps_on(void)
{
    return caps_on;
}

void set_caps_state(bool state)
{
    caps_on = state;
}

bool is_boot_protocol(void)
{
    return keyboard_protocol == 0;
}

void set_boot_protocol(bool boot)
{
    keyboard_protocol = boot ? 0 : 1;
}

int get_hid_report_desc(const uint8_t** report_start, size_t* report_len, int arr_len)
{
    if (!report_start || !report_len || arr_len < HID_REPORT_DESC_MAX_NUM) {
        return 0;
    }

    int report_count = 0;
    size_t offset = 0;

    // Boot Keyboard Report Descriptor
    if (report_count < arr_len) {
        report_start[report_count] = &desc_hid_report[offset];
        report_len[report_count] = BOOT_KEYBOARD_DESC_SIZE;
        offset += BOOT_KEYBOARD_DESC_SIZE;
        report_count++;
    }

#ifdef NKRO_ENABLE
    // NKRO Keyboard Report Descriptor
    if (report_count < arr_len) {
        report_start[report_count] = &desc_hid_report[offset];
        report_len[report_count] = NKRO_KEYBOARD_DESC_SIZE;
        offset += NKRO_KEYBOARD_DESC_SIZE;
        report_count++;
    }
#endif

    return report_count;
}

void enable_usb_hid(void)
{
    tinyusb_config_t tusb_cfg = {
        .device_descriptor = NULL,
        .configuration_descriptor = descriptor_hid_default,
        .string_descriptor = descriptor_str_default,
        .string_descriptor_count = 5,
        .external_phy = false
    };

    keymap_config.nkro = 0;

    ESP_ERROR_CHECK(tinyusb_driver_install(&tusb_cfg));
}

void set_nkro_flag(bool nkro)
{
    keymap_config.nkro = nkro ? 1 : 0;
}

bool get_nkro_flag(void)
{
    return keymap_config.nkro ? true : false;
}

uint8_t const *tud_hid_descriptor_report_cb(uint8_t itf)
{
    if (itf == ITF_NUM_BOOT_KB)
        return desc_boot_report;
    else
        return desc_hid_report;
}

uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t* buffer, uint16_t reqlen)
{
    // Not needed
    (void)instance;
    (void)report_id;
    (void)report_type;
    (void)buffer;
    (void)reqlen;
    return 0;
}

// Invoked when received SET_REPORT control request or
// received data on OUT endpoint ( Report ID = 0, Type = 0 )
void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t const* buffer, uint16_t bufsize)
{
    (void)instance;

    if (report_type == HID_REPORT_TYPE_OUTPUT)
    {
        // report_id is 0, it means it is in boot protocol mode
        if (report_id == 0 || report_id == REPORT_ID_KEYBOARD || report_id == REPORT_ID_NKRO)
        {
            // bufsize should be (at least) 1
            if (bufsize < 1)
                return;

            uint8_t const kbd_leds = buffer[0];

            if (kbd_leds & KEYBOARD_LED_CAPSLOCK)
            {
                // Capslock On: disable blink, turn led on
                caps_on = true;
            }
            else
            {
                // Caplocks Off: back to normal blink
                caps_on = false;
            }
        }
    }
}

void tud_hid_set_protocol_cb(uint8_t instance, uint8_t protocol)
{
    ESP_LOGI(TAG, "Set keyboard protocol from %d to %d", keyboard_protocol, protocol);
    keyboard_protocol = protocol;
}
