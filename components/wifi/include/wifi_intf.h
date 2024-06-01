#ifndef __WIFI_AP_H__
#define __WIFI_AP_H__
#include "esp_wifi.h"

esp_err_t wifi_update(wifi_mode_t mode, const char* ssid, const char* passwd);
esp_err_t wifi_init(wifi_mode_t mode, const char* ssid, const char* passwd);
esp_err_t wifi_stop(void);

esp_err_t get_ip_addr(char* buf, int buf_size);

#endif
