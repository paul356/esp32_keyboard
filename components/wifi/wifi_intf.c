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

#include <string.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "wifi_intf.h"

#define EXAMPLE_ESP_WIFI_CHANNEL   1
#define EXAMPLE_MAX_STA_CONN       2

#ifndef MIN
#define MIN(x, y) (((x) < (y)) ? (x) : (y))
#endif

#ifndef MAC2STR
#define MAC2STR(a) (a)[0], (a)[1], (a)[2], (a)[3], (a)[4], (a)[5]
#define MACSTR "%02x:%02x:%02x:%02x:%02x:%02x"
#endif

#define WIFI_STATION_CONNECTED 0x1
#define WIFI_HOTSPOT_ENABLED   0x2

static const char *TAG = "WiFi";
static bool netif_inited = false;
static uint8_t wifi_state_bits  = 0;
static esp_ip4_addr_t ip_addr;

static void wifi_event_handler(void* arg,
                               esp_event_base_t event_base,
                               int32_t event_id,
                               void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
        ESP_LOGI(TAG, "station "MACSTR" join, AID=%d",
                 MAC2STR(event->mac), event->aid);
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
        ESP_LOGI(TAG, "station "MACSTR" leave, AID=%d",
                 MAC2STR(event->mac), event->aid);
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_CONNECTED) {
        wifi_event_sta_connected_t* event = (wifi_event_sta_connected_t*)event_data;
        ESP_LOGI(TAG, "station is connected to %s", event->ssid);
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
        ESP_LOGI(TAG, "station connects to ssid");
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_event_sta_disconnected_t* event = (wifi_event_sta_disconnected_t*)event_data;
        ESP_LOGI(TAG, "station is disconnected from %s", event->ssid);
        ip_addr.addr = 0;
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "station get ip:" IPSTR, IP2STR(&event->ip_info.ip));
        ip_addr = event->ip_info.ip;
    }


}

static esp_err_t start_softap(const char* ssid, const char* passwd)
{
    esp_err_t ret = ESP_OK;
    wifi_config_t wifi_config = {
        .ap = {
            .ssid = "",
            .ssid_len = 0,
            .channel = EXAMPLE_ESP_WIFI_CHANNEL,
            .password = "",
            .max_connection = EXAMPLE_MAX_STA_CONN,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK
        },
    };

    memcpy(wifi_config.ap.ssid, ssid, MIN(strlen(ssid), sizeof(wifi_config.ap.ssid)));
    wifi_config.ap.ssid_len = MIN(strlen(ssid), sizeof(wifi_config.ap.ssid));
    snprintf((char *)wifi_config.ap.password, sizeof(wifi_config.ap.password), "%s", passwd);

    if (strlen(passwd) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ret = esp_wifi_set_mode(WIFI_MODE_AP);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = esp_wifi_set_config(WIFI_IF_AP, &wifi_config);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = esp_wifi_start();
    if (ret != ESP_OK) {
        return ret;
    }

    wifi_state_bits |= WIFI_STATION_CONNECTED;
    ESP_LOGI(TAG, "wifi_init_softap finished. SSID:%s password:XXX channel:%d",
             ssid, EXAMPLE_ESP_WIFI_CHANNEL);
    // hard coded 192.168.4.1
    ip_addr.addr = 0x0104a8c0;

    return ESP_OK;
}

static esp_err_t start_station(const char* ssid, const char* passwd)
{
    esp_err_t ret = ESP_OK;
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = "",
            .password = "",
            .scan_method = WIFI_ALL_CHANNEL_SCAN,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            .sae_pwe_h2e = WPA3_SAE_PWE_BOTH
        },
    };

    snprintf((char *)wifi_config.sta.ssid, sizeof(wifi_config.sta.ssid), "%s", ssid);
    snprintf((char *)wifi_config.sta.password, sizeof(wifi_config.sta.password), "%s", passwd);

    if (strlen(passwd) == 0) {
        wifi_config.sta.threshold.authmode = WIFI_AUTH_OPEN;
    }

    ret = esp_wifi_set_mode(WIFI_MODE_STA);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = esp_wifi_start();
    if (ret != ESP_OK) {
        return ret;
    }

    wifi_state_bits |= WIFI_STATION_CONNECTED;
    ESP_LOGI(TAG, "wifi_init_sta finished. SSID:%s password:XXX", ssid);

    return ESP_OK;
}

esp_err_t wifi_stop(void)
{
    if (wifi_state_bits & WIFI_STATION_CONNECTED) {
        esp_wifi_disconnect();
    }

    if (wifi_state_bits != 0) {
        // stop softap first
        esp_err_t ret = esp_wifi_stop();
        if (ret != ESP_OK) {
            return ret;
        }
    }

    if (wifi_state_bits & WIFI_STATION_CONNECTED) {
        wifi_state_bits ^= WIFI_STATION_CONNECTED;
    } else if (wifi_state_bits & WIFI_HOTSPOT_ENABLED) {
        wifi_state_bits ^= WIFI_HOTSPOT_ENABLED;
    }
    ip_addr.addr = 0;

    return ESP_OK;
}

static esp_err_t start_wifi(wifi_mode_t mode, const char* ssid, const char* passwd)
{
    switch (mode) {
    case WIFI_MODE_NULL:
        return ESP_OK;
    case WIFI_MODE_STA:
        return start_station(ssid, passwd);
    default:
        return start_softap(ssid, passwd);
    }
}

esp_err_t wifi_update(wifi_mode_t mode, const char* new_ssid, const char* new_passwd)
{
    esp_err_t ret = wifi_stop();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "stop wifi failed");
    }

    return start_wifi(mode, new_ssid, new_passwd);
}

esp_err_t wifi_init(wifi_mode_t mode, const char* ssid, const char* passwd)
{
    esp_err_t ret;

    if (!netif_inited) {
        ret = esp_netif_init();
        if (ret != ESP_OK) {
            return ret;
        }
    
        ret = esp_event_loop_create_default();
        if (ret != ESP_OK) {
            return ret;
        }

        // create drivers for AP+STA
        esp_netif_create_default_wifi_ap();
        esp_netif_create_default_wifi_sta();

        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        ret = esp_wifi_init(&cfg);
        if (ret != ESP_OK) {
            return ret;
        }

        ret = esp_event_handler_instance_register(WIFI_EVENT,
                                                  ESP_EVENT_ANY_ID,
                                                  &wifi_event_handler,
                                                  NULL,
                                                  NULL);
        if (ret != ESP_OK) {
            return ret;
        }

        ret = esp_event_handler_instance_register(IP_EVENT,
                                                  IP_EVENT_STA_GOT_IP,
                                                  &wifi_event_handler,
                                                  NULL,
                                                  NULL);
        if (ret != ESP_OK) {
            return ret;
        }

        // mark flag
        netif_inited = true;
    }

    return start_wifi(mode, ssid, passwd);
}

esp_err_t get_ip_addr(char* buf, int buf_size)
{
    int required_size = snprintf(buf, buf_size, IPSTR, IP2STR(&ip_addr));
    if (required_size + 1 > buf_size) {
        return ESP_ERR_INVALID_ARG;
    }

    return ESP_OK;
}
