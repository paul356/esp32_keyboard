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
#include "drv_loop.h"
#include "esp_timer.h"
#include "esp_log.h"

#define TAG "pwr_mgmt"

// Define event base for power management events
ESP_EVENT_DEFINE_BASE(PWR_MGMT_EVENTS);

typedef enum {
    PWR_MGMT_PERIODIC_EVENT,
    PWR_MGMT_EVENT_MAX
} pwr_mgmt_event_id_t;

// Idle state timestamps (in microseconds from esp_timer_get_time())
static uint64_t last_activity_time = 0;
static bool idle_detection_initialized = false;

// Power management state tracking
static idle_state_t last_processed_state = IDLE_STATE_ACTIVE;

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
 * It checks the current idle state and takes appropriate power-saving actions
 * when state transitions occur.
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

    // Only take action when state changes (reentrant safe)
    if (current_state != last_processed_state) {
        ESP_LOGI(TAG, "Idle state changed: %s -> %s (idle for %lu ms)",
                 state_to_string(last_processed_state),
                 state_to_string(current_state),
                 (unsigned long)idle_get_time_ms());

        // Apply display power management based on idle state
        display_power_mgmt_update(current_state);

        // Apply LED power management based on idle state
        led_power_mgmt_update(current_state);

        last_processed_state = current_state;
    }
}
