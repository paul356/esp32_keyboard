#include <string.h>
#include "function_key.h"
#include "quantum.h"
#include "wifi_intf.h"

// need a string for each function key code
const char* function_key_strs[FUNCTION_KEY_NUM] = {
    "info",
    "intro"
};

static char ip_str[20];

const char *get_function_key_str(uint16_t keycode)
{
    if (keycode >= FUNCTION_KEY_MIN && keycode <= FUNCTION_KEY_MAX) {
        return function_key_strs[keycode - FUNCTION_KEY_MIN];
    } else {
        return NULL;
    }
}

esp_err_t parse_function_key_str(const char* str, uint16_t *keycode)
{
    for (int i = 0; i < sizeof(function_key_strs) / sizeof(function_key_strs[0]); i++) {
        if (strcmp(str, function_key_strs[i]) == 0) {
            *keycode = FUNCTION_KEY_MIN + (uint16_t)i;
            return ESP_OK;
        }
    }
    return ESP_FAIL;
}

static esp_err_t print_device_info()
{
    esp_err_t err = ESP_OK;
    
    SEND_STRING("IP address: ");

    err = get_ip_addr(ip_str, sizeof(ip_str));
    if (err != ESP_OK) {
        return err;
    }

    SEND_STRING(ip_str);
    
    return ESP_OK;
}

static esp_err_t print_help_info()
{
    esp_err_t err = ESP_OK;

    SEND_STRING("Dear User,\\n  Hope you enjoy using this smart keyboard. You can visit http://");

    err = get_ip_addr(ip_str, sizeof(ip_str));
    if (err != ESP_OK) {
        return err;
    }

    SEND_STRING(ip_str);
    SEND_STRING("/ to explore the features. You can\\n");
    SEND_STRING("  1. Customize the keyboard layout.\\n");
    SEND_STRING("  2. Define your own key combination as macros.\\n");
    SEND_STRING("  3. Set up WiFi configuration.\\n");
    SEND_STRING("  4. Upgrade your keyboard by uploading a new firmware.\\n");
    SEND_STRING("  By default this keyboard is in HotSpot mode. You can also connect your keyboard to your home AP by switching the WiFi mode and providing ssid and password on the Status tab.");
    return ESP_OK;
}

esp_err_t process_function_key(uint16_t keycode)
{
    switch (keycode) {
    case FUNCTION_KEY_DEVICE_INFO:
        print_device_info();
        break;
    case FUNCTION_KEY_INTRO:
        print_help_info();
        break;
    default:
        SEND_STRING("Unkonwn function key!");
    }

    return ESP_OK;
}
