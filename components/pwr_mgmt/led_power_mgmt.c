/*
 * LED Power Management Integration
 *
 * This module integrates LED power controls with the idle detection system
 * to optimize power consumption based on user activity.
 *
 * Copyright 2025
 */

#include "idle_detection.h"
#include "led_ctrl.h"
#include "led_drv.h"
#include "function_control.h"
#include "esp_log.h"
#include "miscs.h"

#define TAG "led_pwr"

// LED power profile configuration
typedef struct {
    uint8_t brightness;     // LED brightness (0-100), 0 = off
    bool rmt_enabled;       // Whether RMT hardware should be enabled
} led_power_profile_t;

// Power profiles for USB-powered mode (no power saving)
static const led_power_profile_t usb_profiles[] = {
    [IDLE_STATE_ACTIVE]     = { .brightness = 100, .rmt_enabled = true  },
    [IDLE_STATE_SHORT]      = { .brightness = 100, .rmt_enabled = true  },
    [IDLE_STATE_LONG]       = { .brightness = 0,   .rmt_enabled = false },
};

// Power profiles for battery-powered mode (aggressive power saving)
static const led_power_profile_t battery_profiles[] = {
    [IDLE_STATE_ACTIVE]     = { .brightness = 60,  .rmt_enabled = true  },
    [IDLE_STATE_SHORT]      = { .brightness = 30,  .rmt_enabled = true  },
    [IDLE_STATE_LONG]       = { .brightness = 0,   .rmt_enabled = false },
};

// Battery-level based brightness scaling (applied on top of idle profiles)
static uint8_t get_battery_brightness_scale(uint8_t battery_percentage)
{
    if (battery_percentage >= 50) {
        return 100;  // No reduction
    } else if (battery_percentage >= 30) {
        return 80;   // Slight reduction
    } else if (battery_percentage >= 20) {
        return 60;   // Moderate reduction
    } else if (battery_percentage >= 10) {
        return 40;   // Significant reduction
    } else {
        return 20;   // Critical - minimal brightness
    }
}

static void apply_led_profile(const led_power_profile_t *profile, uint8_t battery_scale)
{
    esp_err_t ret;
    bool led_enabled = is_led_enabled();
    bool rmt_enabled = led_ctrl_rmt_enabled();

    // Calculate scaled brightness (battery scale applied on top of profile brightness)
    uint8_t scaled_brightness = (profile->brightness * battery_scale) / 100;

    // In configuration LED is on, but it could be turned off by power management.
    if (profile->rmt_enabled && led_enabled && !rmt_enabled) {
        // Need to enable RMT hardware
        ret = led_ctrl_enable_rmt();
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to enable LED RMT: %s", esp_err_to_name(ret));
            return; // Can't proceed without RMT enabled
        } else {
            ESP_LOGI(TAG, "LED RMT enabled (power save mode ended)");
        }
    }

    // Now set brightness and update LEDs (RMT is enabled if needed)
    ret = led_ctrl_set_brightness(scaled_brightness);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set brightness: %s", esp_err_to_name(ret));
    }

    if (scaled_brightness == 0) {
        // If brightness is 0, explicitly clear LEDs to ensure they're off
        ret = led_ctrl_clear();
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to clear LEDs: %s", esp_err_to_name(ret));
        }
        ESP_LOGI(TAG, "LEDs turned OFF");
    } else {
        ESP_LOGI(TAG, "LED brightness set to %u%% (scaled from %u%%)",
                 scaled_brightness, profile->brightness);
    }

    if (!profile->rmt_enabled && rmt_enabled) {
        // Need to disable RMT hardware
        ret = led_ctrl_disable_rmt();
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to disable LED RMT: %s", esp_err_to_name(ret));
        } else {
            ESP_LOGI(TAG, "LED RMT disabled (power save mode)");
        }
    }
}

void led_power_mgmt_update(idle_state_t idle_state)
{
    // Determine if running on USB or battery
    bool usb_powered = miscs_is_usb_powered();

    // Select appropriate profile set
    const led_power_profile_t *profile;
    uint8_t battery_scale = 100;

    if (usb_powered) {
        // USB powered - use full brightness profiles
        profile = &usb_profiles[idle_state];
        ESP_LOGD(TAG, "Using USB power profile for state: %s", idle_get_state_string());
    } else {
        // Battery powered - use power saving profiles with battery scaling
        profile = &battery_profiles[idle_state];

        uint8_t battery_percentage = 0;
        if (miscs_get_battery_percentage(&battery_percentage, false) == ESP_OK) {
            battery_scale = get_battery_brightness_scale(battery_percentage);
            ESP_LOGD(TAG, "Using battery profile (%u%%) for state: %s, scale: %u%%",
                     battery_percentage, idle_get_state_string(), battery_scale);
        } else {
            ESP_LOGW(TAG, "Failed to read battery level, using default scale");
        }
    }

    // Apply the selected profile
    apply_led_profile(profile, battery_scale);
}
