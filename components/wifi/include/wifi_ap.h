#ifndef __WIFI_AP_H__
#define __WIFI_AP_H__

esp_err_t wifi_update_softap(const char* ssid, const char* passwd);

esp_err_t wifi_init_softap(const char* ssid, const char* passwd);

esp_err_t wifi_stop_softap(void);

#endif
