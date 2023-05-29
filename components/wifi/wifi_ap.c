#include <string.h>
#include <stdbool.h>
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_funcs.h"
#include "nvs.h"

#define EXAMPLE_ESP_WIFI_CHANNEL   1
#define EXAMPLE_MAX_STA_CONN       2

#ifndef MIN
#define MIN(x, y) (((x) < (y)) ? (x) : (y))
#endif

static const char *TAG = "SOFTAP";
static bool netif_inited = false;
static bool wifiap_started = false;

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                    int32_t event_id, void* event_data)
{
    if (event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
        ESP_LOGI(TAG, "station "MACSTR" join, AID=%d",
                 MAC2STR(event->mac), event->aid);
    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
        ESP_LOGI(TAG, "station "MACSTR" leave, AID=%d",
                 MAC2STR(event->mac), event->aid);
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
    memcpy(wifi_config.ap.password, passwd, MIN(strlen(passwd), sizeof(wifi_config.ap.password)));

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

    wifiap_started = true;
    ESP_LOGI(TAG, "wifi_init_softap finished. SSID:%s password:%s channel:%d",
             ssid, passwd, EXAMPLE_ESP_WIFI_CHANNEL);

    return ESP_OK;
}

esp_err_t wifi_stop_softap(void)
{
    // stop softap first
    esp_err_t ret = esp_wifi_stop();
    if (ret != ESP_OK) {
        return ret;
    }

    wifiap_started = false;
    return ESP_OK;
}

esp_err_t wifi_update_softap(const char* new_ssid, const char* new_passwd)
{
    if (wifiap_started) {
        // stop softap first
        esp_err_t ret = wifi_stop_softap();
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "stop wifi failed");
        }
    }

    return start_softap(new_ssid, new_passwd);
}

esp_err_t wifi_init_softap(const char* ssid, const char* passwd)
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

        esp_netif_create_default_wifi_ap();

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

        // mark flag
        netif_inited = true;
    }

    return start_softap(ssid, passwd);
}
