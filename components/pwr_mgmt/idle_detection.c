/*
 * Idle Detection System for ESP32 Keyboard
 *
 * This module provides a centralized mechanism for detecting user inactivity
 * to enable power-saving features across all components.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Copyright 2025
 */

#include "idle_detection.h"
#include "display_power_mgmt.h"
#include "led_power_mgmt.h"
#include "ble_power_mgmt.h"
#include "drv_loop.h"
#include "esp_timer.h"
#include "esp_sleep.h"
#include "esp_log.h"
#include "miscs.h"
#include "hal_ble.h"
#include "config.h"
#include "driver/gpio.h"
#include "matrix.h"

#define TAG "pwr_mgmt"

// Define event base for power management events
ESP_EVENT_DEFINE_BASE(PWR_MGMT_EVENTS);

typedef enum {
    PWR_MGMT_PERIODIC_EVENT,
    PWR_MGMT_EVENT_MAX
} pwr_mgmt_event_id_t;

static const gpio_num_t row_pins[MATRIX_ROWS] = MATRIX_ROW_PINS;
static const gpio_num_t col_pins[MATRIX_COLS] = MATRIX_COL_PINS;

// Idle state timestamps (in microseconds from esp_timer_get_time())
static uint64_t last_activity_time = 0;
static bool idle_detection_initialized = false;

// Power management state tracking
static idle_state_t last_processed_state = IDLE_STATE_ACTIVE;
static bool last_usb_powered_state = true;  // Track last known power source

// Forward declarations
static const char* state_to_string(idle_state_t state);
static void pwr_mgmt_event_handler(void* event_handler_arg, esp_event_base_t event_base,
                                   int32_t event_id, void* event_data);

// Implementation of idle detection functions (declarations in idle_detection.h)

void idle_detection_init(void) {
    last_activity_time = esp_timer_get_time();
    idle_detection_initialized = true;

    // Register event handler with drv_loop
    esp_err_t ret = drv_loop_register_handler(PWR_MGMT_EVENTS, PWR_MGMT_PERIODIC_EVENT,
                                              pwr_mgmt_event_handler, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register power management event handler: %s", esp_err_to_name(ret));
    }

    ESP_LOGI(TAG, "Idle detection system initialized");
}

void idle_detection_reset(void) {
    last_activity_time = esp_timer_get_time();

    // Only take action when state changes (reentrant safe)
    if (last_processed_state != IDLE_STATE_ACTIVE)
        pwr_mgmt_process();
}

uint32_t idle_get_time_ms(void) {
    if (!idle_detection_initialized) {
        return 0;
    }
    uint64_t current_time = esp_timer_get_time();
    uint64_t elapsed_us = current_time - last_activity_time;
    return (uint32_t)(elapsed_us / 1000); // Convert to milliseconds
}

idle_state_t idle_get_state(void) {
    uint32_t idle_ms = idle_get_time_ms();

    if (idle_ms < IDLE_THRESHOLD_SHORT) {
        return IDLE_STATE_ACTIVE;
    } else if (idle_ms < IDLE_THRESHOLD_LONG) {
        return IDLE_STATE_SHORT;
    } else {
        return IDLE_STATE_LONG;
    }
}

bool idle_exceeds_threshold(uint32_t threshold_ms) {
    return idle_get_time_ms() >= threshold_ms;
}

const char* idle_get_state_string(void) {
    return state_to_string(idle_get_state());
}

// Helper function to convert idle state to string
static const char* state_to_string(idle_state_t state) {
    switch (state) {
        case IDLE_STATE_ACTIVE:     return "ACTIVE";
        case IDLE_STATE_SHORT:      return "SHORT_IDLE";
        case IDLE_STATE_LONG:       return "LONG_IDLE";
        default:                     return "UNKNOWN";
    }
}

/**
 * @brief Configure GPIO wakeup sources for light sleep
 *
 * This function configures the keyboard matrix GPIOs for wakeup from light sleep:
 * - Sets all MATRIX_ROW_PINS to output low (ground side of the matrix)
 * - Configures all MATRIX_COL_PINS as wakeup sources with low-level trigger
 * When a key is pressed, one or more column pins will be pulled low through
 * the diode and row pin, triggering the wakeup.
 */
// Matrix pin definitions for wakeup configuration
static esp_err_t configure_gpio_wakeup(void) {
    esp_err_t ret;

    // Set all row pins to output low
    // This ensures that when a key is pressed, the column pin will be pulled low
    for (int i = 0; i < MATRIX_ROWS; i++) {
        ret = gpio_set_direction(row_pins[i], GPIO_MODE_OUTPUT);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Failed to set row pin %d as output: %s", row_pins[i], esp_err_to_name(ret));
            return ret;
        }

        ret = gpio_set_level(row_pins[i], 0);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Failed to set row pin %d to low: %s", row_pins[i], esp_err_to_name(ret));
            return ret;
        }

        ret = gpio_hold_en(row_pins[i]);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Failed to set row pin %d to low: %s", row_pins[i], esp_err_to_name(ret));
            return ret;
        }
    }

    // Configure all column pins as GPIO wakeup sources with low level trigger
    // When any key is pressed, the corresponding column pin will be pulled low
    for (int i = 0; i < MATRIX_COLS; i++) {
        // Set as input with pull-up (column pins are normally high)
        ret = gpio_set_direction(col_pins[i], GPIO_MODE_INPUT);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Failed to set col pin %d as input: %s", col_pins[i], esp_err_to_name(ret));
            return ret;
        }

        ret = gpio_set_pull_mode(col_pins[i], GPIO_PULLUP_ONLY);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Failed to set pull-up on col pin %d: %s", col_pins[i], esp_err_to_name(ret));
            return ret;
        }

        // Enable wakeup on low level (when key is pressed and pulls the column low)
        ret = gpio_wakeup_enable(col_pins[i], GPIO_INTR_LOW_LEVEL);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Failed to enable wakeup on col pin %d: %s", col_pins[i], esp_err_to_name(ret));
            return ret;
        }
    }

    // Enable GPIO wakeup
    ret = esp_sleep_enable_gpio_wakeup();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to enable GPIO wakeup: %s", esp_err_to_name(ret));
        return ret;
    } else {
        ESP_LOGI(TAG, "GPIO wakeup configured: %d row pins set to low, %d col pins as wakeup sources",
                 MATRIX_ROWS, MATRIX_COLS);
    }

    return ESP_OK;
}

static void deconfigure_gpio_wakeup()
{
    for (int i = 0; i < MATRIX_ROWS; i++)
    {
        gpio_hold_dis(row_pins[i]);
    }
    // Reinitialize matrix pins to their normal scanning configuration
    matrix_init();
}

void pwr_mgmt_process(void) {
    // Post event to drv_loop for asynchronous processing
    esp_err_t err = drv_loop_post_event(PWR_MGMT_EVENTS, PWR_MGMT_PERIODIC_EVENT, NULL, 0, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Fail to post power management events: %s", esp_err_to_name(err));
    }
}

/**
 * @brief Event handler for power management periodic event
 *
 * This handler is called when PWR_MGMT_PERIODIC_EVENT is posted to the drv_loop.
 * It checks the current idle state and power source, taking appropriate power-saving
 * actions when state transitions or power source changes occur.
 */
static void pwr_mgmt_event_handler(void* event_handler_arg, esp_event_base_t event_base,
                                   int32_t event_id, void* event_data) {
    // Verify this is our event
    if (event_base != PWR_MGMT_EVENTS || event_id != PWR_MGMT_PERIODIC_EVENT) {
        return;
    }

    if (!idle_detection_initialized) {
        return;
    }

    idle_state_t current_state = idle_get_state();
    bool current_usb_powered = miscs_is_usb_powered();

    // Check for power source change
    bool power_source_changed = (current_usb_powered != last_usb_powered_state);

    if (power_source_changed) {
        ESP_LOGI(TAG, "Power source changed: %s -> %s (current state: %s)",
                 last_usb_powered_state ? "USB" : "BATTERY",
                 current_usb_powered ? "USB" : "BATTERY",
                 state_to_string(current_state));

        last_usb_powered_state = current_usb_powered;
    }

    // Take action when state changes OR power source changes
    if (current_state != last_processed_state || power_source_changed) {
        if (current_state != last_processed_state && !power_source_changed) {
            ESP_LOGI(TAG, "Idle state changed: %s -> %s (idle for %lu ms)",
                     state_to_string(last_processed_state),
                     state_to_string(current_state),
                     (unsigned long)idle_get_time_ms());
        }

        // Apply display power management based on idle state
        display_power_mgmt_update(current_state);

        // Apply LED power management based on idle state
        led_power_mgmt_update(current_state);

        // Apply BLE power management based on idle state
        ble_power_mgmt_update(current_state);

        last_processed_state = current_state;
    }

    // When last state is the long idle state and power is from battery, we will enter light sleep.
    if (last_processed_state == IDLE_STATE_LONG && last_usb_powered_state == false && is_ble_ready() == false)
    {
        ESP_LOGI(TAG, "Entering light sleep with GPIO wakeup...");

        // Disable all wakeup sources first
        esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);

        // Configure GPIO wakeup sources (matrix keyboard pins)
        esp_err_t ret = configure_gpio_wakeup();
        if (ret != ESP_OK)
        {
            ESP_LOGW(TAG, "Skip light sleep due to a gpio configuration error.");
        } else {
            // Enter light sleep
            ret = esp_light_sleep_start();
            if (ret != ESP_OK)
            {
                ESP_LOGW(TAG, "Failed to enter light sleep: %s", esp_err_to_name(ret));
            }
            else
            {
                ESP_LOGI(TAG, "Woke up from light sleep");

                deconfigure_gpio_wakeup();

                // Reset idle detection timer after wakeup
                idle_detection_reset();
            }
        }
    }
}
