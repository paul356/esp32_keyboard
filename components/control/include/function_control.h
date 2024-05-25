#ifndef __FUNCTION_CONTROL_H__
#define __FUNCTION_CONTROL_H__

#include <stdbool.h>
#include "esp_err.h"
#include "esp_wifi.h"

esp_err_t update_wifi_state(wifi_mode_t mode, const char* ssid, const char* passwd);
bool is_wifi_enabled();

esp_err_t update_ble_state(bool enabled, const char* name);
bool is_ble_enabled();

esp_err_t update_usb_state(bool enabled);
bool is_usb_enabled();

esp_err_t restore_saved_state();

#endif
