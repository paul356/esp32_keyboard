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
#include <string.h>
#include "function_control.h"
#include "nvs_io.h"
#include "nvs.h"
#include "esp_log.h"
#include "esp_random.h"
#include "hal_ble.h"
#include "wifi_intf.h"
#include "memory_debug.h"
#include "led_ctrl.h"
#include "drv_loop.h"
#include "keymap_server.h"

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
    LED,
    FUNCTION_BUTT
} function_control_e;

typedef struct _control_state_t {
    struct {
        bool enabled;
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
    struct {
        bool enabled;
        led_pattern_type_e pattern;
    } led;
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

static void set_default_wifi_enabled(control_state_t* config);
static void set_default_wifi_mode(control_state_t* config);
static void set_default_wifi_ssid(control_state_t* config);
static void set_default_wifi_passwd(control_state_t* config);
static void set_default_ble_enabled(control_state_t* config);
static void set_default_ble_name(control_state_t* config);
static void set_default_usb_enabled(control_state_t* config);
static void set_default_led_enabled(control_state_t* config);
static void set_default_led_pattern(control_state_t* config);

static esp_err_t load_config_value(function_control_e function, const char* item_name, void* config_value, size_t* len);
static esp_err_t save_config_value(function_control_e function, const char* item_name, const void* config_value, size_t len);

CONFIG_VALUE_GETTER_SETTER(wifi, enabled)
CONFIG_VALUE_GETTER_SETTER(wifi, mode)
CONFIG_VALUE_GETTER_SETTER(wifi, ssid)
CONFIG_VALUE_GETTER_SETTER(wifi, passwd)
CONFIG_VALUE_GETTER_SETTER(ble, enabled)
CONFIG_VALUE_GETTER_SETTER(ble, name)
CONFIG_VALUE_GETTER_SETTER(usb, enabled)
CONFIG_VALUE_GETTER_SETTER(led, enabled)
CONFIG_VALUE_GETTER_SETTER(led, pattern)

static config_item_t wifi_config[] = {
    {"enabled", &load_wifi_enabled, &save_wifi_enabled, set_default_wifi_enabled},
    {"mode",    &load_wifi_mode,    &save_wifi_mode,    set_default_wifi_mode},
    {"ssid",    &load_wifi_ssid,    &save_wifi_ssid,    set_default_wifi_ssid},
    {"passwd",  &load_wifi_passwd,  &save_wifi_passwd,  set_default_wifi_passwd}
};

static config_item_t ble_config[] = {
    {"enabled", &load_ble_enabled,  &save_ble_enabled, set_default_ble_enabled},
    {"name",    &load_ble_name,     &save_ble_name,    set_default_ble_name}
};

static config_item_t usb_config[] = {
    {"enabled", &load_usb_enabled,  &save_usb_enabled, set_default_usb_enabled}
};

static config_item_t led_config[] = {
    {"enabled", &load_led_enabled,  &save_led_enabled, set_default_led_enabled},
    {"pattern", &load_led_pattern,  &save_led_pattern, set_default_led_pattern}
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
    },
    {
        ARRAY_LEN(led_config), led_config
    }
};

static control_state_t function_state;

// BLE Control Events - for async BLE state changes via drv_loop
ESP_EVENT_DEFINE_BASE(BLE_CONTROL_EVENTS);

enum {
    BLE_STATE_CHANGE_EVENT,  // Event to enable/disable BLE
};

#define BLE_CONTROL_POST_TIMEOUT pdMS_TO_TICKS(100)

// Event data structure for BLE state changes
typedef struct {
    bool enabled;
    char name[MAX_CONFIG_VALUE_LEN];
} ble_state_event_data_t;

// Forward declaration
static void ble_state_event_handler(void* handler_args, esp_event_base_t base, int32_t id, void* event_data);

static const char* function_config_prefix[] = {"WIFI_", "BLE_", "USB_", "LED_"};

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

void set_default_wifi_enabled(control_state_t* state)
{
    state->wifi.enabled = false;
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

void set_default_usb_enabled(control_state_t* state)
{
    state->usb.enabled = true;
}

void set_default_led_enabled(control_state_t* state)
{
    state->led.enabled = true;
}

void set_default_led_pattern(control_state_t* state)
{
    state->led.pattern = LED_PATTERN_HIT_KEY;
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
    log_memory_usage("BEFORE restore_saved_state");

    // Register BLE state event handler with drv_loop for async BLE on/off control
    esp_err_t ret = drv_loop_register_handler(BLE_CONTROL_EVENTS, BLE_STATE_CHANGE_EVENT,
                                              ble_state_event_handler, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register BLE state event handler: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "BLE state event handler registered");

    ret = recover_persisted_config();
    if (ret != ESP_OK) {
        return ret;
    }

    log_memory_usage("After recover_persisted_config");
    if (function_state.wifi.enabled) {
        ret = wifi_update(function_state.wifi.mode, function_state.wifi.ssid, function_state.wifi.passwd);
        if (ret != ESP_OK) {
            return ret;
        }
    }
    log_memory_usage("After wifi_init");

    if (function_state.ble.enabled) {
        // Init BLE anyway
        ret = init_ble_device(function_state.ble.name);
        if (ret != ESP_OK)
        {
            return ret;
        }
    }

    log_memory_usage("After init_ble_device");

    if (function_state.led.enabled) {
        ret = led_ctrl_set_pattern(function_state.led.pattern, 0, 0);
    } else {
        ret = led_ctrl_set_pattern(LED_PATTERN_OFF, 0, 0);
    }
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Fail to set led ctrl pattern, enabled=%d, pattern=%d, ret=%d", function_state.led.enabled, function_state.led.pattern, ret);
    }

    return ESP_OK;
}

esp_err_t update_wifi_switch(bool flag)
{
    function_state.wifi.enabled = flag;

    esp_err_t err;
    if (flag) {
        err = wifi_update(function_state.wifi.mode, function_state.wifi.ssid, function_state.wifi.passwd);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "fail to enable wifi");
            return err;
        }

        // Start file server when WiFi is enabled
        err = start_file_server();
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "fail to start file server");
            // Don't fail the WiFi enable operation if file server fails
        }
    } else {
        // Stop file server when WiFi is disabled
        err = stop_file_server();
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "fail to stop file server");
            // Continue with WiFi stop even if file server stop fails
        }

        err = wifi_stop();
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "fail to disable wifi");
            return err;
        }
    }

    return update_persisted_config(WIFI);
}

esp_err_t update_wifi_mode(wifi_mode_t mode, const char* ssid, const char* passwd)
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

    if (mode == WIFI_MODE_NULL) {
        // If the mode is set to NULL, we stop the wifi service
        function_state.wifi.enabled = false;
    } else {
        function_state.wifi.enabled = true;
    }

    if (function_state.wifi.enabled) {
        err = wifi_update(mode, function_state.wifi.ssid, function_state.wifi.passwd);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "fail to update wifi state, mode=%d err=%d", mode, err);
            return err;
        }

        // Start file server when WiFi is enabled
        err = start_file_server();
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "fail to start file server");
            // Don't fail the WiFi operation if file server fails
        }
    } else {
        // Stop file server when WiFi is disabled
        err = stop_file_server();
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "fail to stop file server");
            // Continue with WiFi stop even if file server stop fails
        }

        err = wifi_stop();
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "fail to stop wifi, err=%d", err);
            return err;
        }
    }

    return update_persisted_config(WIFI);
}

esp_err_t update_usb_state(bool enabled)
{
    function_state.usb.enabled = enabled;

    return update_persisted_config(USB);
}

bool is_wifi_enabled(void)
{
    return function_state.wifi.enabled;
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

static void update_ble_state(ble_state_event_data_t* ble_data)
{

    bool last_enabled = function_state.ble.enabled;
    bool new_enabled = ble_data->enabled;

    function_state.ble.enabled = new_enabled;

    if (strlen(ble_data->name) > 0) {
        snprintf(function_state.ble.name, sizeof(function_state.ble.name), "%s", ble_data->name);
    }

    esp_err_t ret = update_persisted_config(BLE);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "fail to update BLE state, err=%d", ret);
        return;
    }

    ESP_LOGI(TAG, "BLE state change event: last=%d, new=%d, name=%s",
             last_enabled, new_enabled, ble_data->name);

    // Initialize BLE if transitioning from disabled to enabled
    if (new_enabled && !last_enabled) {
        esp_err_t ret = init_ble_device(ble_data->name);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to init BLE device: %s", esp_err_to_name(ret));
            return;
        }
        ESP_LOGI(TAG, "BLE device initialized successfully");
    }

    // Deinitialize BLE if transitioning from enabled to disabled
    if (last_enabled && !new_enabled) {
        esp_err_t ret = deinit_ble_device();
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to deinit BLE device: %s", esp_err_to_name(ret));
            return;
        }
        ESP_LOGI(TAG, "BLE device deinitialized successfully");
    }
}

/**
 * @brief Event handler for BLE state changes
 *
 * This handler is called asynchronously by drv_loop when a BLE state change event is posted.
 * It performs the actual BLE initialization or deinitialization in the drv_loop task context.
 */
static void ble_state_event_handler(void* handler_args, esp_event_base_t base, int32_t id, void* event_data)
{
    if (id != BLE_STATE_CHANGE_EVENT) {
        return;
    }

    update_ble_state((ble_state_event_data_t*)event_data);
}

esp_err_t update_ble_state_async(bool enabled, const char* name)
{
    // Prepare event data
    ble_state_event_data_t event_data = {
        .enabled = enabled
    };
    if (name && strlen(name) > 0) {
        snprintf(event_data.name, sizeof(event_data.name), "%s", name);
    } else {
        snprintf(event_data.name, sizeof(event_data.name), "%s", function_state.ble.name);
    }

    // Post event to drv_loop for async processing
    esp_err_t ret = drv_loop_post_event(BLE_CONTROL_EVENTS, BLE_STATE_CHANGE_EVENT, &event_data, sizeof(event_data),
                                        BLE_CONTROL_POST_TIMEOUT);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to post BLE state change event: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "BLE state change event posted: enabled=%d, name=%s", enabled, event_data.name);

    return ESP_OK;
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

esp_err_t update_led_switch(bool flag)
{
    function_state.led.enabled = flag;

    if (flag) {
        led_ctrl_set_pattern(function_state.led.pattern, 0, 0);
    } else {
        led_ctrl_set_pattern(LED_PATTERN_OFF, 0, 0);
    }

    return update_persisted_config(LED);
}

esp_err_t update_led_pattern(led_pattern_type_e pattern)
{
    if (pattern < LED_PATTERN_OFF || pattern >= LED_PATTERN_MAX) {
        ESP_LOGE(TAG, "Invalid LED pattern: %d", pattern);
        return ESP_ERR_INVALID_ARG;
    }

    if (pattern == LED_PATTERN_OFF) {
        // If the pattern is OFF, we can just disable the LED
        function_state.led.enabled = false;
    } else {
        function_state.led.enabled = true;
    }

    function_state.led.pattern = pattern;
    led_ctrl_set_pattern(pattern, 0, 0);

    return update_persisted_config(LED);
}

bool is_led_enabled(void)
{
    return function_state.led.enabled;
}

led_pattern_type_e get_led_pattern(void)
{
    return function_state.led.pattern;
}

