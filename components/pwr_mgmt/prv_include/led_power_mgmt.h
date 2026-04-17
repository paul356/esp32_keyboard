/*
 * LED Power Management Integration
 *
 * This module integrates LED power controls with the idle detection system.
 *
 * Copyright (C) 2025 panhao356@gmail.com
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef LED_POWER_MGMT_H
#define LED_POWER_MGMT_H

#include "idle_detection.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Update LED power settings based on idle state
 *
 * This function should be called whenever the idle state changes.
 * It automatically adjusts LED brightness and on/off state based on
 * the current idle level and power source (USB/battery).
 *
 * @param idle_state Current idle state from idle detection system
 */
void led_power_mgmt_update(idle_state_t idle_state);

#ifdef __cplusplus
}
#endif

#endif // LED_POWER_MGMT_H
