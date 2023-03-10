#ifndef __FUNCTION_CONTROL_H__
#define __FUNCTION_CONTROL_H__

#include <stdbool.h>
#include "esp_err.h"

esp_err_t toggle_wifi_state(bool enabled);
bool is_wifi_enabled();

esp_err_t toggle_ble_hid_state(bool enabled);
bool is_ble_hid_enabled();

esp_err_t toggle_usb_hid_state(bool enabled);
bool is_usb_hid_enabled();

esp_err_t restore_function_state();

#endif
