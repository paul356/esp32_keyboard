/*
 * Copyright (C) 2026 panhao356@gmail.com
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef MISCS_H
#define MISCS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_err.h"
#include "driver/gpio.h"
#include "esp_adc/adc_oneshot.h"

/**
 * Rotary Encoder Direction
 */
typedef enum {
    MISCS_ENCODER_STOPPED = 0,
    MISCS_ENCODER_CW = 1,
    MISCS_ENCODER_CCW = -1
} miscs_encoder_direction_t;

/**
 * @brief Initialize miscellaneous peripheral hardware
 *
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t miscs_init(void);

/**
 * @brief Deinitialize miscellaneous peripheral hardware
 *
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t miscs_deinit(void);

/**
 * @brief Check if device is powered by USB
 *
 * @return true if USB power is detected (GPIO6 high), false if battery powered
 */
bool miscs_is_usb_powered(void);

/**
 * @brief Check if battery is charging
 *
 * @return true if battery is charging (GPIO7 low), false if not charging
 */
bool miscs_is_battery_charging(void);

/**
 * @brief Get battery charge percentage
 *
 * This function reads the battery voltage and calculates the charge percentage
 * based on typical Li-Ion battery voltage range (3.0V - 4.2V).
 *
 * @param percentage Pointer to store battery percentage (0-100)
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t miscs_get_battery_percentage(uint8_t *percentage, bool log_voltage);

/**
 * @brief Get current encoder position
 *
 * @return int32_t Current encoder position (increments/decrements based on rotation)
 */
int32_t miscs_encoder_get_position(void);

/**
 * @brief Reset encoder position to zero
 */
void miscs_encoder_reset_position(void);

/**
 * @brief Get encoder direction from last movement
 *
 * @return miscs_encoder_direction_t Direction of last encoder movement
 */
miscs_encoder_direction_t miscs_encoder_get_direction(void);

/**
 * @brief Check if encoder button is pressed
 *
 * @return true if button is pressed (GPIO45 low), false if not pressed
 */
bool miscs_encoder_button_pressed(void);

#ifdef __cplusplus
}
#endif

#endif // MISCS_H
