/*
 * Display Power Management Integration
 *
 * This module integrates display power controls with the idle detection system.
 *
 * Copyright 2025
 */

#ifndef DISPLAY_POWER_MGMT_H
#define DISPLAY_POWER_MGMT_H

#include "idle_detection.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Update display power settings based on idle state
 *
 * This function should be called whenever the idle state changes.
 * It automatically adjusts display brightness, refresh rate, and on/off
 * state based on the current idle level and power source (USB/battery).
 *
 * @param idle_state Current idle state from idle detection system
 */
void display_power_mgmt_update(idle_state_t idle_state);

/**
 * @brief Force display to wake up and become fully active
 *
 * Call this when user interaction is detected to immediately restore
 * full display brightness and refresh rate, bypassing the idle state.
 * The idle detection system should be reset separately.
 */
void display_power_mgmt_force_wake(void);

#ifdef __cplusplus
}
#endif

#endif // DISPLAY_POWER_MGMT_H
