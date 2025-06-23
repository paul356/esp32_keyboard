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
#define LED_DRV_NUM_LEDS        70      // Number of LEDs in the strip
#define LED_DRV_GPIO_PIN        4       // GPIO pin for LED strip data

/**
 * LED Color Structure (RGB format)
 */
typedef struct {
    uint8_t red;    ///< Red component (0-255)
    uint8_t green;  ///< Green component (0-255)
    uint8_t blue;   ///< Blue component (0-255)
} led_drv_color_t;

/**
 * @brief Initialize the LED strip driver
 * 
 * This function initializes the RMT peripheral for driving WS2812 LEDs.
 * It configures GPIO4 as the output pin for the LED data signal.
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
 * 
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t led_drv_clear(void);

/**
 * @brief Set a single LED color
 * 
 * This function sets the color of a single LED at the specified index.
 * Note: You must call led_drv_update() to actually send the data to the strip.
 * 
 * @param index LED index (0 to LED_DRV_NUM_LEDS-1)
 * @param color LED color to set
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
 * @brief Set all LEDs to the same color
 * 
 * This function sets all LEDs in the strip to the same color and updates the strip.
 * 
 * @param color Color to set for all LEDs
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t led_drv_set_all(led_drv_color_t color);

#ifdef __cplusplus
}
#endif

#endif // LED_DRV_H
