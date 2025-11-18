/*
 * BLE Power Management Integration
 *
 * This module integrates BLE power controls with the idle detection system
 * to optimize power consumption based on user activity.
 *
 * Copyright 2025
 */

#include "idle_detection.h"
#include "hal_ble.h"
#include "function_control.h"
#include "esp_log.h"
#include "miscs.h"

#define TAG "ble_pwr"

// BLE power profile configuration
typedef struct {
    bool ble_enabled;       // Whether BLE should be enabled
} ble_power_profile_t;

// Power profiles for USB-powered mode (no power saving)
static const ble_power_profile_t usb_profiles[] = {
    [IDLE_STATE_ACTIVE]     = { .ble_enabled = true  },
    [IDLE_STATE_SHORT]      = { .ble_enabled = true  },
    [IDLE_STATE_LONG]       = { .ble_enabled = true  },
};

// Power profiles for battery-powered mode (aggressive power saving)
static const ble_power_profile_t battery_profiles[] = {
    [IDLE_STATE_ACTIVE]     = { .ble_enabled = true  },
    [IDLE_STATE_SHORT]      = { .ble_enabled = true  },
    [IDLE_STATE_LONG]       = { .ble_enabled = false },  // Disable BLE to save power
};

static void apply_ble_profile(const ble_power_profile_t *profile)
{
    esp_err_t ret;
    bool ble_enabled = is_ble_enabled();
    bool ble_ready = is_ble_ready();

    // In configuration BLE is on, but it could be turned off by power management.
    if (profile->ble_enabled && ble_enabled && !ble_ready) {
        // Need to enable BLE
        const char* ble_name = get_ble_name();
        ret = init_ble_device(ble_name);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to enable BLE: %s", esp_err_to_name(ret));
        } else {
            ESP_LOGI(TAG, "BLE enabled (power save mode ended)");
        }
    } else if (!profile->ble_enabled && ble_ready) {
        // Need to disable BLE
        ret = deinit_ble_device();
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to disable BLE: %s", esp_err_to_name(ret));
        } else {
            ESP_LOGI(TAG, "BLE disabled (power save mode)");
        }
    }
}

void ble_power_mgmt_update(idle_state_t idle_state)
{
    // Determine if running on USB or battery
    bool usb_powered = miscs_is_usb_powered();

    // Select appropriate profile set
    const ble_power_profile_t *profile;

    if (usb_powered) {
        // USB powered - keep BLE always enabled
        profile = &usb_profiles[idle_state];
        ESP_LOGD(TAG, "Using USB power profile for state: %s", idle_get_state_string());
    } else {
        // Battery powered - apply power saving profiles
        profile = &battery_profiles[idle_state];
        ESP_LOGD(TAG, "Using battery profile for state: %s", idle_get_state_string());
    }

    // Apply the selected profile
    apply_ble_profile(profile);
}
