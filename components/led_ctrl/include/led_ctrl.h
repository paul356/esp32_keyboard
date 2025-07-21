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

#ifdef __cplusplus
}
#endif

#endif // LED_CTRL_H
