/*
 * Idle Detection System for ESP32 Keyboard
 *
 * This module provides a centralized mechanism for detecting user inactivity
 * to enable power-saving features across all components.
 *
 * Copyright 2025
 */

#ifndef IDLE_DETECTION_H
#define IDLE_DETECTION_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Idle state enumeration
typedef enum {
    IDLE_STATE_ACTIVE = 0,      // Currently active (< 5s idle)
    IDLE_STATE_SHORT,           // Short idle (5s - 30s)
    IDLE_STATE_LONG             // Long idle (> 30s)
} idle_state_t;

// Idle thresholds (in milliseconds) - exposed for component-specific checks
#define IDLE_THRESHOLD_ACTIVE     0       // 0ms - Currently active
#define IDLE_THRESHOLD_SHORT      5000    // 5 seconds - Just went idle
#define IDLE_THRESHOLD_LONG       30000   // 30 seconds - Long idle

/**
 * @brief Initialize idle detection system
 *
 * Must be called once during system initialization before using
 * any other idle detection functions.
 */
void idle_detection_init(void);

/**
 * @brief Update activity timestamp
 *
 * Call this function whenever any user interaction occurs
 * (key press, encoder movement, touch, etc.)
 * This resets the idle timer to zero.
 */
void idle_detection_reset(void);

/**
 * @brief Get time elapsed since last activity
 *
 * @return Time in milliseconds since last user activity
 */
uint32_t idle_get_time_ms(void);

/**
 * @brief Get current idle state
 *
 * Returns one of the predefined idle states based on
 * how long the system has been idle.
 *
 * @return Current idle state enum value
 */
idle_state_t idle_get_state(void);

/**
 * @brief Check if system is idle for at least the specified duration
 *
 * @param threshold_ms Idle threshold in milliseconds
 * @return true if idle time exceeds or equals threshold, false otherwise
 *
 * Example usage:
 *   if (idle_exceeds_threshold(30000)) {
 *       // System has been idle for at least 30 seconds
 *       turn_off_display();
 *   }
 */
bool idle_exceeds_threshold(uint32_t threshold_ms);

/**
 * @brief Get idle state as string (for logging/debugging)
 *
 * @return String representation of current idle state
 *         ("ACTIVE", "SHORT_IDLE", "MEDIUM_IDLE", "LONG_IDLE", "VERY_LONG_IDLE")
 */
const char* idle_get_state_string(void);

/**
 * @brief Process power management based on idle state
 *
 * This function should be called periodically (e.g., in the main loop).
 * It monitors the idle state and takes appropriate power-saving actions
 * when state transitions occur. The function is reentrant - actions are
 * only performed once when entering a new idle state.
 *
 * Example usage:
 *   while (1) {
 *       // ... handle keyboard events ...
 *       pwr_mgmt_process();
 *       vTaskDelay(1);
 *   }
 */
void pwr_mgmt_process(void);

#ifdef __cplusplus
}
#endif

#endif // IDLE_DETECTION_H
