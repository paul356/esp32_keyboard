/*
 * BLE Power Management Integration
 *
 * This module integrates BLE power controls with the idle detection system
 * to optimize power consumption based on user activity.
 *
 * Copyright (C) 2025 panhao356@gmail.com
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef BLE_POWER_MGMT_H
#define BLE_POWER_MGMT_H

#include "idle_detection.h"

/**
 * @brief Update BLE power management based on idle state
 *
 * This function is called when the system's idle state changes.
 * It applies appropriate BLE power management strategies based on the current
 * idle state and power source (USB or battery).
 *
 * @param idle_state Current idle state of the system
 */
void ble_power_mgmt_update(idle_state_t idle_state);

#endif /* BLE_POWER_MGMT_H */
