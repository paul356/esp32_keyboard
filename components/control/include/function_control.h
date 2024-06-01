#ifndef __FUNCTION_CONTROL_H__
#define __FUNCTION_CONTROL_H__

#include <stdbool.h>
#include "esp_err.h"
#include "esp_wifi.h"

esp_err_t update_wifi_state(wifi_mode_t mode, const char* ssid, const char* passwd);
wifi_mode_t get_wifi_mode(void);
const char* get_wifi_ssid(void);

const char* wifi_mode_to_str(wifi_mode_t mode);
wifi_mode_t str_to_wifi_mode(const char* str);

esp_err_t update_ble_state(bool enabled, const char* name);
bool is_ble_enabled(void);
const char* get_ble_name(void);

esp_err_t update_usb_state(bool enabled);
bool is_usb_enabled(void);

esp_err_t restore_saved_state(void);

#endif
