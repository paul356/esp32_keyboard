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
    LED_PATTERN_HIT_KEY,              // Solid color
    LED_PATTERN_MAX
} led_pattern_type_t;

/**
 * Pattern configuration structure
 */
typedef struct {
    led_pattern_type_t pattern;     // Pattern type
    led_drv_color_t primary_color;  // Primary color for pattern
    led_drv_color_t secondary_color; // Secondary color (for gradients, etc.)
    uint8_t brightness;             // Overall brightness (0-255)
    uint32_t param1;                // Pattern-specific parameter 1
    uint32_t param2;                // Pattern-specific parameter 2
} led_pattern_config_t;

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
esp_err_t led_ctrl_set_pattern(led_pattern_type_t pattern_type, uint32_t param1, uint32_t param2);

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
