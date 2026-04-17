/*
 * Copyright (C) 2026 panhao356@gmail.com
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef LED_CTRL_H
#define LED_CTRL_H

#include "esp_err.h"
#include "esp_event.h"
#include "led_drv.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * LED pattern types
 */
typedef enum {
    LED_PATTERN_OFF,                // All LEDs off
    LED_PATTERN_HIT_KEY,            // Solid color
    LED_PATTERN_MAX
} led_pattern_type_e;

/**
 * @brief Initialize the LED control component
 *
 * This function initializes the LED control component, sets up event handlers
 * with drv_loop, and starts the pattern execution system.
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t led_ctrl_init(void);

/**
 * @brief Set the current LED pattern
 *
 * This function sets the LED pattern configuration. The change will take effect
 * on the next pattern update cycle.
 *
 * @param pattern_type Type of the LED pattern to set
 * @param param1 Pattern-specific parameter 1 (e.g., speed, duration)
 * @param param2 Pattern-specific parameter 2 (e.g., intensity, color variation)
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t led_ctrl_set_pattern(led_pattern_type_e pattern_type, uint32_t param1, uint32_t param2);

/**
 * @brief Get the current LED pattern configuration
 *
 * This function retrieves the current LED pattern configuration including
 * pattern type and parameters.
 *
 * @param pattern_type Pointer to store the current pattern type
 * @param param1 Pointer to store parameter 1 value
 * @param param2 Pointer to store parameter 2 value
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG if any pointer is NULL, ESP_ERR_INVALID_STATE if not initialized
 */
esp_err_t led_ctrl_get_pattern(led_pattern_type_e *pattern_type, uint32_t *param1, uint32_t *param2);

/**
 * @brief Report a keystroke event to the LED control system
 *
 * This function should be called whenever a key is pressed or released.
 * It will trigger appropriate LED effects based on the current pattern.
 *
 * @param scancode Key scancode
 * @param row Key position row
 * @param col Key position column
 * @param pressed True if pressed, false if released
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t led_ctrl_keystroke(uint8_t row, uint8_t col, bool pressed);

/**
 * @brief Set global LED brightness
 *
 * This function sets the global brightness level for all LEDs.
 * The brightness is applied as a multiplier when setting LED colors.
 * Use brightness=0 to effectively turn off all LEDs.
 *
 * @param brightness Brightness level (0-100, where 0 is off and 100 is full brightness)
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG if brightness > 100, ESP_ERR_INVALID_STATE if not initialized
 */
esp_err_t led_ctrl_set_brightness(uint8_t brightness);

/**
 * @brief Get current global LED brightness
 *
 * This function retrieves the current global brightness level.
 *
 * @param brightness Pointer to store the current brightness level (0-100)
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG if pointer is NULL, ESP_ERR_INVALID_STATE if not initialized
 */
esp_err_t led_ctrl_get_brightness(uint8_t *brightness);

/**
 * @brief Clear all LEDs
 *
 * This function immediately turns off all LEDs by setting them to black
 * and updating the LED strip. This is useful for power management or
 * resetting the LED state.
 *
 * @return ESP_OK on success, ESP_ERR_INVALID_STATE if not initialized
 */
esp_err_t led_ctrl_clear(void);

/**
 * @brief Enable the LED RMT hardware
 *
 * This function posts an event to enable the RMT hardware for LED control.
 * The operation is performed asynchronously in the drv_loop context.
 * After enabling, LED updates will be transmitted to the LED strips.
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t led_ctrl_enable_rmt(void);

/**
 * @brief Disable the LED RMT hardware
 *
 * This function posts an event to disable the RMT hardware to save power.
 * The operation is performed asynchronously in the drv_loop context.
 * After disabling, LED update requests will be queued but not transmitted
 * until RMT is re-enabled.
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t led_ctrl_disable_rmt(void);

/**
 * @brief Check if the LED RMT hardware is enabled
 *
 * This function returns the current state of the RMT hardware used for LED control.
 *
 * @return true if RMT hardware is enabled, false otherwise
 */
bool led_ctrl_rmt_enabled(void);

#ifdef __cplusplus
}
#endif

#endif // LED_CTRL_H
