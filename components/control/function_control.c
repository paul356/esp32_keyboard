#include <stddef.h>
#include "function_control.h"
#include "nvs_funcs.h"
#include "esp_log.h"
#include "hal_ble.h"
#include "wifi_ap.h"

#define FUNCTION_CTRL_NAMESPACE "FUNC_CTRL"
#define TAG "FUNC_CTRL"

typedef struct _function_control_t {
    bool is_usb_hid_enabled;
    bool is_ble_hid_enabled;
    bool is_wifi_enabled;
    bool persisted; // false when program is inited
} function_control_t;

static function_control_t function_state = {false, false, false, false};

static esp_err_t toggle_usb_hid_state_internal(bool enabled, bool save_state);
static esp_err_t toggle_ble_hid_state_internal(bool enabled, bool save_state);
static esp_err_t toggle_wifi_state_internal(bool enabled, bool save_state);

static esp_err_t save_function_state(const char* state_prefix)
{
    esp_err_t ret = nvs_write_blob(FUNCTION_CTRL_NAMESPACE, FUNCTION_CTRL_NAMESPACE, &function_state, sizeof(function_control_t));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Fail to save %s function state, ret=%d", state_prefix, ret);            
    }

    return ret;    
}

static esp_err_t init_default_state()
{
    function_control_t init_state = {true, false, true, true};
    esp_err_t ret;

    ret = toggle_usb_hid_state_internal(init_state.is_usb_hid_enabled, false);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = toggle_ble_hid_state_internal(init_state.is_ble_hid_enabled, false);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = toggle_wifi_state_internal(init_state.is_wifi_enabled, false);
    if (ret != ESP_OK) {
        return ret;
    }

    function_state = init_state;
    ret = save_function_state(__FUNCTION__);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Fail to persist default function state to nvs storage");
    }

    return ESP_OK;
}

esp_err_t restore_saved_state()
{
    // Check if it is already persisted
    if (function_state.persisted) {
        return ESP_OK;
    }

    function_control_t persisted_state;
    size_t size = sizeof(function_control_t);
    // Use namespace as KV key
    esp_err_t ret = nvs_read_blob(FUNCTION_CTRL_NAMESPACE, FUNCTION_CTRL_NAMESPACE, &persisted_state, &size);
    if (ret != ESP_OK && ret != ESP_ERR_NOT_FOUND) {
        ESP_LOGE(TAG, "Fail to read blob %s, ret=%d", FUNCTION_CTRL_NAMESPACE, ret);
        return ret;
    } else if (ret == ESP_ERR_NOT_FOUND) {
        // Use default value
        return init_default_state();
    }

    ret = toggle_usb_hid_state_internal(persisted_state.is_usb_hid_enabled, false);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = toggle_ble_hid_state_internal(persisted_state.is_ble_hid_enabled, false);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = toggle_wifi_state_internal(persisted_state.is_wifi_enabled, false);
    if (ret != ESP_OK) {
        return ret;
    }

    if (!persisted_state.persisted) {
        ESP_LOGW(TAG, "Invalid \"persisted\" flag in nvs storage");
        persisted_state.persisted = true;
    }

    function_state = persisted_state;

    return ESP_OK;
}

#define CHECK_PERSISTED_FLAG()                                          \
    do {                                                                \
        if (!function_state.persisted) {                                \
            esp_err_t ret = restore_saved_state();                      \
            if (ret != ESP_OK) {                                        \
                ESP_LOGE(TAG, "%s: Fail to get saved state, ret=%d", __FUNCTION__, ret); \
                return ret;                                             \
            }                                                           \
        }                                                               \
    } while (0)
    

static esp_err_t toggle_wifi_state_internal(bool enabled, bool save_state)
{
    CHECK_PERSISTED_FLAG();

    // Already enabled or disabled, just skip
    if (function_state.is_wifi_enabled == enabled) {
        return ESP_OK;
    }

    if (enabled) {
        esp_err_t ret = wifi_init_softap();
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "%s: Fail to enable softap, ret=%d", __FUNCTION__, ret);
            return ret;
        }
    } else {
        // TODO: add disable logic
    }

    function_state.is_wifi_enabled = enabled;

    if (save_state) {
        return save_function_state("wifi");
    } else {
        return ESP_OK;
    }
}

esp_err_t toggle_wifi_state(bool enabled)
{
    return toggle_wifi_state_internal(enabled, true);
}

static esp_err_t toggle_ble_hid_state_internal(bool enabled, bool save_state)
{
    CHECK_PERSISTED_FLAG();

    // Already enabled or disabled, just skip
    if (function_state.is_ble_hid_enabled == enabled) {
        return ESP_OK;
    }

    if (enabled) {
        esp_err_t ret = halBLEInit(1, 1, 1, 0);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "%s: Fail to enable ble init, ret=%d", __FUNCTION__, ret);
            return ret;
        }
    } else {
        // TODO: add disable logic
    }

    function_state.is_ble_hid_enabled = enabled;

    if (save_state) {
        return save_function_state("ble_hid");
    } else {
        return ESP_OK;
    }    
}

esp_err_t toggle_ble_hid_state(bool enabled)
{
    return toggle_ble_hid_state_internal(enabled, true);
}

static esp_err_t toggle_usb_hid_state_internal(bool enabled, bool save_state)
{
    CHECK_PERSISTED_FLAG();

    // Already enabled or disabled, just skip
    if (function_state.is_usb_hid_enabled == enabled) {
        return ESP_OK;
    }

    function_state.is_ble_hid_enabled = enabled;

    if (save_state) {
        return save_function_state("ble_hid");
    } else {
        return ESP_OK;
    }    
}

esp_err_t toggle_usb_hid_state(bool enabled)
{
    return toggle_usb_hid_state_internal(enabled, true);
}


