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
 * @brief Configure GPIO pin for miscellaneous hardware
 * 
 * @param gpio_num GPIO pin number
 * @param mode GPIO mode (input/output)
 * @param pull_mode GPIO pull mode
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t miscs_gpio_config(gpio_num_t gpio_num, gpio_mode_t mode, gpio_pull_mode_t pull_mode);

/**
 * @brief Set GPIO pin level
 * 
 * @param gpio_num GPIO pin number
 * @param level GPIO level (0 or 1)
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t miscs_gpio_set_level(gpio_num_t gpio_num, uint32_t level);

/**
 * @brief Get GPIO pin level
 * 
 * @param gpio_num GPIO pin number
 * @return int GPIO level (0 or 1), -1 on error
 */
int miscs_gpio_get_level(gpio_num_t gpio_num);

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
 * @brief Read battery voltage via ADC
 * 
 * @param voltage_mv Pointer to store voltage reading in millivolts
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t miscs_read_battery_voltage(uint32_t *voltage_mv);

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

#ifdef __cplusplus
}
#endif

#endif // MISCS_H
