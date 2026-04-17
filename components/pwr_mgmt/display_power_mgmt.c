/*
 * Display Power Management Integration
 *
 * This module integrates display power controls with the idle detection system
 * to optimize power consumption based on user activity.
 *
 * Copyright (C) 2025 panhao356@gmail.com
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "idle_detection.h"
#include "keyboard_gui.h"
#include "esp_log.h"
#include "miscs.h"

#define TAG "display_pwr"

// Display refresh rate configurations for different idle states
typedef struct {
    uint32_t refresh_period_ms;  // LVGL update period
    uint8_t brightness;          // Display brightness (0-100)
    bool display_on;             // Display on/off state
} display_power_profile_t;

// Power profiles for USB-powered mode (no power saving)
static const display_power_profile_t usb_profiles[] = {
    [IDLE_STATE_ACTIVE]     = { .refresh_period_ms = 250,  .brightness = 100, .display_on = true },
    [IDLE_STATE_SHORT]      = { .refresh_period_ms = 250,  .brightness = 100, .display_on = true },
    [IDLE_STATE_LONG]       = { .refresh_period_ms = 500,  .brightness = 60,  .display_on = true },
};

// Power profiles for battery-powered mode (aggressive power saving)
static const display_power_profile_t battery_profiles[] = {
    [IDLE_STATE_ACTIVE]     = { .refresh_period_ms = 250,  .brightness = 80,  .display_on = true },
    [IDLE_STATE_SHORT]      = { .refresh_period_ms = 500,  .brightness = 60,  .display_on = true },
    [IDLE_STATE_LONG]       = { .refresh_period_ms = 0,    .brightness = 0,   .display_on = false },
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

static void apply_display_profile(const display_power_profile_t *profile, uint8_t battery_scale)
{
    esp_err_t ret;

    // Apply display on/off state
    ret = keyboard_gui_display_on_off(profile->display_on);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set display state: %s", esp_err_to_name(ret));
    }

    // Apply brightness (scaled by battery level if on battery)
    uint8_t scaled_brightness = (profile->brightness * battery_scale) / 100;
    ret = keyboard_gui_set_brightness(scaled_brightness);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to set brightness: %s", esp_err_to_name(ret));
    }

    // Apply refresh rate
    ret = keyboard_gui_set_update_rate(profile->refresh_period_ms);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set update rate: %s", esp_err_to_name(ret));
    }

    // Log the applied profile
    if (profile->display_on) {
        ESP_LOGI(TAG, "Display profile applied: %lu ms refresh, %u%% brightness (scaled from %u%%)",
                 (unsigned long)profile->refresh_period_ms,
                 (profile->brightness * battery_scale) / 100,
                 profile->brightness);
    } else {
        ESP_LOGI(TAG, "Display turned OFF");
    }
}

void display_power_mgmt_update(idle_state_t idle_state)
{
    // Determine if running on USB or battery
    bool usb_powered = miscs_is_usb_powered();

    // Select appropriate profile set
    const display_power_profile_t *profile;
    uint8_t battery_scale = 100;

    if (usb_powered) {
        // USB powered - use full performance profiles
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
    apply_display_profile(profile, battery_scale);
}
