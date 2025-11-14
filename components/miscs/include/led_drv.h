#ifndef LED_DRV_H
#define LED_DRV_H

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_err.h"
#include <stdint.h>

/**
 * LED Strip Configuration
 */
#define LED_DRV_NUM_LEDS        61      // Total number of LEDs across all strips
#define LED_DRV_NUM_STRIPS      3       // Number of LED strips

// LED strip segments
#define LED_DRV_STRIP1_START    0       // Strip 1: LEDs 0-19
#define LED_DRV_STRIP1_END      19
#define LED_DRV_STRIP1_GPIO     4

#define LED_DRV_STRIP2_START    20      // Strip 2: LEDs 20-40
#define LED_DRV_STRIP2_END      40
#define LED_DRV_STRIP2_GPIO     44

#define LED_DRV_STRIP3_START    41      // Strip 3: LEDs 41-60
#define LED_DRV_STRIP3_END      60
#define LED_DRV_STRIP3_GPIO     43

/**
 * LED Color Structure (GRB format - optimized for WS2812)
 * Note: Colors are stored in GRB order to match WS2812 requirements
 */
typedef struct {
    uint8_t green;  ///< Green component (0-255)
    uint8_t red;    ///< Red component (0-255)
    uint8_t blue;   ///< Blue component (0-255)
} led_drv_color_t;

/**
 * Helper macros for creating colors (input in RGB order for convenience)
 */
#define LED_COLOR_RGB(r, g, b) ((led_drv_color_t){.green = (g), .red = (r), .blue = (b)})
#define LED_COLOR_BLACK       LED_COLOR_RGB(0, 0, 0)
#define LED_COLOR_WHITE       LED_COLOR_RGB(255, 255, 255)
#define LED_COLOR_RED         LED_COLOR_RGB(255, 0, 0)
#define LED_COLOR_GREEN       LED_COLOR_RGB(0, 255, 0)
#define LED_COLOR_BLUE        LED_COLOR_RGB(0, 0, 255)

/**
 * @brief Initialize the LED strip driver
 *
 * This function initializes the RMT peripheral for driving WS2812 LEDs
 * across multiple strips on different GPIO pins.
 *
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t led_drv_init(void);

/**
 * @brief Deinitialize the LED strip driver
 *
 * This function cleans up the RMT peripheral and frees allocated resources.
 *
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t led_drv_deinit(void);

/**
 * @brief Clear all LEDs (turn them off)
 *
 * This function turns off all LEDs in the strip by setting them to black (0,0,0).
 */
void led_drv_clear(void);

/**
 * @brief Set a single LED color
 *
 * This function sets the color of a single LED at the specified index.
 * Colors are stored internally in GRB format for optimal WS2812 performance.
 * Note: You must call led_drv_update() to actually send the data to the strip.
 *
 * @param index LED index (0 to LED_DRV_NUM_LEDS-1)
 * @param color LED color to set (use LED_COLOR_RGB macro for convenience)
 * @return esp_err_t ESP_OK on success, ESP_ERR_INVALID_ARG if index is out of range
 */
esp_err_t led_drv_set_led(uint16_t index, led_drv_color_t color);

/**
 * @brief Update the LED strip with buffered data
 *
 * This function sends the current LED buffer to the strip.
 * Use this after calling led_drv_set_led() one or more times.
 *
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t led_drv_update(void);

/**
 * @brief Enable the LED strip RMT peripheral
 *
 * This function enables the RMT peripheral for all LED strips, allowing
 * LED updates to be transmitted. The peripheral should be enabled before
 * calling led_drv_update().
 *
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t led_drv_enable(void);

/**
 * @brief Disable the LED strip RMT peripheral
 *
 * This function disables the RMT peripheral for all LED strips to save power.
 * Call this when LEDs are not in use (e.g., during long idle periods).
 * The LED buffer is preserved, so you can re-enable and update later.
 *
 * Note: Call led_drv_clear() and led_drv_update() before disabling to ensure
 * LEDs are turned off before the peripheral is disabled.
 *
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t led_drv_disable(void);

#ifdef __cplusplus
}
#endif

#endif // LED_DRV_H
