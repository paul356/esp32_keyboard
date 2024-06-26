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

#include <stddef.h>
#include "function_control.h"
#include "nvs_funcs.h"
#include "nvs.h"
#include "esp_log.h"
#include "esp_random.h"
#include "hal_ble.h"
#include "wifi_intf.h"

#define TAG "FUNC_CTRL"
#define FUNCTION_CTRL_NAMESPACE "FUNC_CTRL"

#define MAX_CONFIG_VALUE_LEN 16
#define MAX_CONFIG_KEY_LEN 16

#define WIFI_SSID_INIT_TEMPLATE "KB-%lu"
#define WIFI_SSID_ID_MOD 1000

#define BLE_NAME_INIT_TEMPLATE WIFI_SSID_INIT_TEMPLATE
#define BLE_NAME_ID_MOD WIFI_SSID_ID_MOD

#define ARRAY_LEN(arr) (sizeof(arr)/sizeof(arr[0]))

#define CONFIG_VALUE_GETTER_SETTER(function, item_name) \
static esp_err_t load_##function##_##item_name(function_control_e funct, const char* name, control_state_t* config) \
{ \
    size_t len = sizeof(config->function.item_name); \
    return load_config_value(funct, name, &(config->function.item_name), &len); \
} \
static esp_err_t save_##function##_##item_name(function_control_e funct, const char* name, const control_state_t* config) \
{ \
    return save_config_value(funct, name, &(config->function.item_name), sizeof(config->function.item_name)); \
}

typedef enum _function_control_e {
    WIFI,
    BLE,
    USB,
    FUNCTION_BUTT
} function_control_e;

typedef struct _control_state_t {
    struct {
        wifi_mode_t mode;
        char ssid[MAX_CONFIG_VALUE_LEN];
        char passwd[MAX_CONFIG_VALUE_LEN];
    } wifi;
    struct {
        bool enabled;
        char name[MAX_CONFIG_VALUE_LEN];
    } ble;
    struct {
        bool enabled;
    } usb;
} control_state_t;

typedef esp_err_t (*read_item_func)(function_control_e function, const char* item_name, control_state_t* config);
typedef esp_err_t (*write_item_func)(function_control_e function, const char* item_name, const control_state_t* config);
typedef void (*default_value_func)(control_state_t* config);

typedef struct _config_item_t {
    const char *item_name;
    read_item_func read_func;
    write_item_func write_func;
    default_value_func gen_default;
} config_item_t;

typedef struct _function_config_t {
    int config_num;
    const config_item_t* config_items;
} function_config_t;

static void set_default_wifi_mode(control_state_t* config);
static void set_default_wifi_ssid(control_state_t* config);
static void set_default_wifi_passwd(control_state_t* config);
static void set_default_ble_enabled(control_state_t* config);
static void set_default_ble_name(control_state_t* config);
static void set_default_usb_values(control_state_t* config);

static esp_err_t load_config_value(function_control_e function, const char* item_name, void* config_value, size_t* len);
static esp_err_t save_config_value(function_control_e function, const char* item_name, const void* config_value, size_t len);

CONFIG_VALUE_GETTER_SETTER(wifi, mode)
CONFIG_VALUE_GETTER_SETTER(wifi, ssid)
CONFIG_VALUE_GETTER_SETTER(wifi, passwd)
CONFIG_VALUE_GETTER_SETTER(ble, enabled)
CONFIG_VALUE_GETTER_SETTER(ble, name)
CONFIG_VALUE_GETTER_SETTER(usb, enabled)

static config_item_t wifi_config[] = {
    {"mode",    &load_wifi_mode,    &save_wifi_mode,    set_default_wifi_mode},
    {"ssid",    &load_wifi_ssid,    &save_wifi_ssid,    set_default_wifi_ssid},
    {"passwd",  &load_wifi_passwd,  &save_wifi_passwd,  set_default_wifi_passwd}
};

static config_item_t ble_config[] = {
    {"enabled", &load_ble_enabled,  &save_ble_enabled, set_default_ble_enabled},
    {"name",    &load_ble_name,     &save_ble_name,    set_default_ble_name}
};

static config_item_t usb_config[] = {
    {"enabled", &load_usb_enabled,  &save_usb_enabled, set_default_usb_values}
};

static function_config_t function_config_entries[] = {
    {
        ARRAY_LEN(wifi_config), wifi_config
    },
    {
        ARRAY_LEN(ble_config), ble_config
    },
    {
        ARRAY_LEN(usb_config), usb_config
    }
};

static control_state_t function_state;

static const char* function_config_prefix[] = {"WIFI_", "BLE_", "USB_"};

esp_err_t load_config_value(
    function_control_e function,
    const char* item_name,
    void* config_value,
    size_t* len)
{
    char key[MAX_CONFIG_KEY_LEN];

    snprintf(key, sizeof(key), "%s%s", function_config_prefix[function], item_name);

    return nvs_read_blob(FUNCTION_CTRL_NAMESPACE, key, config_value, len);
}

esp_err_t save_config_value(
    function_control_e function,
    const char* item_name,
    const void* config_value,
    size_t len)
{
    char key[MAX_CONFIG_KEY_LEN];

    snprintf(key, sizeof(key), "%s%s", function_config_prefix[function], item_name);

    return nvs_write_blob(FUNCTION_CTRL_NAMESPACE, key, config_value, len);
}

static unsigned long random_id = 0;
static unsigned long get_default_device_id(void)
{
    if (random_id == 0) {
        random_id = esp_random() % WIFI_SSID_ID_MOD;
    }
    return random_id;
}

void set_default_wifi_mode(control_state_t* state)
{
    state->wifi.mode = WIFI_MODE_AP;
}
void set_default_wifi_ssid(control_state_t* state)
{
    snprintf(state->wifi.ssid, sizeof(state->wifi.ssid), WIFI_SSID_INIT_TEMPLATE, get_default_device_id());
}
void set_default_wifi_passwd(control_state_t* state)
{
    snprintf(state->wifi.passwd, sizeof(state->wifi.passwd), "%s", "1234567890");
}

void set_default_ble_enabled(control_state_t* state)
{
    state->ble.enabled = true;
}
void set_default_ble_name(control_state_t* state)
{
    snprintf(state->ble.name, sizeof(state->ble.name), BLE_NAME_INIT_TEMPLATE, get_default_device_id());
}

void set_default_usb_values(control_state_t* state)
{
    state->usb.enabled = true;
}

static esp_err_t recover_persisted_config()
{
    for (int i = 0; i < ARRAY_LEN(function_config_entries); i++) {
        for (int j = 0; j < function_config_entries[i].config_num; j++) {
            const config_item_t* item = &(function_config_entries[i].config_items[j]);
            const char* name = item->item_name;
            esp_err_t err = item->read_func(i, name, &function_state);
            if (err != ESP_OK && (err != ESP_ERR_NVS_NOT_FOUND || !item->gen_default)) {
                return err;
            } else if (err == ESP_ERR_NVS_NOT_FOUND) {
                item->gen_default(&function_state);
                item->write_func(i, name, &function_state);
            }
        }
    }

    return ESP_OK;
}

static esp_err_t update_persisted_config(function_control_e func)
{
    esp_err_t err = ESP_OK;

    for (int i = 0; i < function_config_entries[func].config_num; i++) {
        const config_item_t* item = &(function_config_entries[func].config_items[i]);
        err = item->write_func(func, item->item_name, &function_state);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "fail to persist %s, err=%d", item->item_name, err);
        }
    }

    return err;
}

esp_err_t restore_saved_state(void)
{
    esp_err_t ret = recover_persisted_config();
    if (ret != ESP_OK) {
        return ret;
    }

    ret = wifi_init(function_state.wifi.mode, function_state.wifi.ssid, function_state.wifi.passwd);
    if (ret != ESP_OK) {
        return ret;
    }

    if (function_state.ble.enabled) {
        ret = halBLEInit(function_state.ble.name);
        if (ret != ESP_OK) {
            return ret;
        }
    }

    return ESP_OK;
}

esp_err_t update_wifi_state(wifi_mode_t mode, const char* ssid, const char* passwd)
{
    esp_err_t err;
    function_state.wifi.mode = mode;

    if (ssid && strlen(ssid) > 0) {
        snprintf(function_state.wifi.ssid, sizeof(function_state.wifi.ssid), "%s", ssid);
    } else {
        ESP_LOGE(TAG, "ssid can't be null");
        return ESP_ERR_INVALID_ARG;
    }

    if (passwd) {
        snprintf(function_state.wifi.passwd, sizeof(function_state.wifi.passwd), "%s", passwd);
    } else {
        ESP_LOGE(TAG, "passwd can't be null");
        return ESP_ERR_INVALID_ARG;
    }

    if (mode != WIFI_MODE_NULL) {
        err = wifi_update(mode, function_state.wifi.ssid, function_state.wifi.passwd);
    } else {
        err = wifi_stop();
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "fail to update wifi state, mode=%d err=%d", mode, err);
        return err;
    }

    return update_persisted_config(WIFI);
}

esp_err_t update_ble_state(bool enabled, const char* name)
{
    // TODO: it is to be implemented.
    return ESP_FAIL;
}

esp_err_t update_usb_state(bool enabled)
{
    function_state.usb.enabled = enabled;

    return update_persisted_config(USB);
}

wifi_mode_t get_wifi_mode(void)
{
    return function_state.wifi.mode;
}

const char* get_wifi_ssid(void)
{
    return function_state.wifi.ssid;
}

const char* wifi_mode_to_str(wifi_mode_t mode)
{
    switch (mode) {
    case WIFI_MODE_NULL:
        return "closed";
    case WIFI_MODE_STA:
        return "client";
    default:
        return "hotspot";
    }
}

wifi_mode_t str_to_wifi_mode(const char* str)
{
    if (strcmp(str, "closed") == 0) {
        return WIFI_MODE_NULL;
    } else if (strcmp(str, "client") == 0) {
        return WIFI_MODE_STA;
    } else {
        return WIFI_MODE_AP;
    }
}

bool is_ble_enabled(void)
{
    return function_state.ble.enabled;
}

const char* get_ble_name(void)
{
    return function_state.ble.name;
}

bool is_usb_enabled(void)
{
    return function_state.usb.enabled;
}

