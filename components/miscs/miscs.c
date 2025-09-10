#include "miscs.h"
#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/**
 * GPIO Pin Definitions
 */
#define MISCS_USB_POWER_GPIO    GPIO_NUM_6   // USB power detection
#define MISCS_BATTERY_CHARGE_GPIO GPIO_NUM_7 // Battery charge status
#define MISCS_BATTERY_ADC_GPIO  GPIO_NUM_5   // Battery voltage ADC
#define MISCS_ENCODER_A_GPIO    GPIO_NUM_21  // Rotary encoder A
#define MISCS_ENCODER_B_GPIO    GPIO_NUM_47  // Rotary encoder B
#define MISCS_ENCODER_BTN_GPIO  GPIO_NUM_45  // Rotary encoder button

/**
 * ADC Configuration
 */
#define MISCS_ADC_UNIT          ADC_UNIT_1
#define MISCS_ADC_ATTEN         ADC_ATTEN_DB_12
#define MISCS_ADC_BITWIDTH      ADC_BITWIDTH_12
#define MISCS_ADC_READ_TIMES    5 // Number of samples to read for battery voltage
#define ENCODER_DAMPEN_RATIO 4

static const char *TAG = "MISCS";

// Static variables for ADC and encoder
static adc_oneshot_unit_handle_t adc1_handle = NULL;
static adc_cali_handle_t adc1_cali_handle = NULL;
static bool adc_calibration_enable = false;

// Encoder state variables
static volatile int32_t encoder_position = 0;
static volatile int32_t encoder_raw_position = 0;  // Raw position before division
static volatile miscs_encoder_direction_t encoder_direction = MISCS_ENCODER_STOPPED;
static volatile uint8_t encoder_last_state = 0;

static esp_err_t miscs_encoder_init(void);
static esp_err_t miscs_gpio_config(gpio_num_t gpio_num, gpio_mode_t mode, gpio_pull_mode_t pull_mode);

// Encoder interrupt handler
static void IRAM_ATTR encoder_isr_handler(void* arg)
{
    uint8_t current_a = gpio_get_level(MISCS_ENCODER_A_GPIO);
    uint8_t current_b = gpio_get_level(MISCS_ENCODER_B_GPIO);
    uint8_t current_state = (current_a << 1) | current_b;

    // State transition lookup table for quadrature decoding
    static const int8_t transition_table[4][4] = {
        { 0, -1,  1,  0},  // From state 00
        { 1,  0,  0, -1},  // From state 01
        {-1,  0,  0,  1},  // From state 10
        { 0,  1, -1,  0}   // From state 11
    };

    int8_t direction = transition_table[encoder_last_state][current_state];
    if (direction != 0) {
        encoder_raw_position += direction;

        // Apply sensitivity divider
        int32_t new_position = encoder_raw_position / ENCODER_DAMPEN_RATIO;
        if (new_position != encoder_position) {
            encoder_position = new_position;
            // It seems direct < 0 is clock wise.
            encoder_direction = (direction < 0) ? MISCS_ENCODER_CW : MISCS_ENCODER_CCW;
        }
    }

    encoder_last_state = current_state;
}

esp_err_t miscs_init(void)
{
    ESP_LOGI(TAG, "Initializing miscellaneous peripheral hardware");

    esp_err_t ret = ESP_OK;

    // Initialize USB power detection GPIO (GPIO6 - input)
    ret = miscs_gpio_config(MISCS_USB_POWER_GPIO, GPIO_MODE_INPUT, GPIO_FLOATING);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure USB power GPIO");
        return ret;
    }

    // Initialize battery charge status GPIO (GPIO7 - input with pull-up)
    ret = miscs_gpio_config(MISCS_BATTERY_CHARGE_GPIO, GPIO_MODE_INPUT, GPIO_PULLUP_ONLY);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure battery charge GPIO");
        return ret;
    }

    // Initialize ADC for battery voltage reading
    adc_oneshot_unit_init_cfg_t init_config1 = {
        .unit_id = MISCS_ADC_UNIT,
    };
    ret = adc_oneshot_new_unit(&init_config1, &adc1_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize ADC unit");
        return ret;
    }

    adc_oneshot_chan_cfg_t config = {
        .bitwidth = MISCS_ADC_BITWIDTH,
        .atten = ADC_ATTEN_DB_12,  // Use updated constant
    };
    ret = adc_oneshot_config_channel(adc1_handle, ADC_CHANNEL_4, &config); // GPIO5 is ADC1_CH4
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure ADC channel");
        return ret;
    }

    // Initialize ADC calibration (mandatory for accurate voltage readings)
    adc_cali_curve_fitting_config_t cali_config = {
        .unit_id = MISCS_ADC_UNIT,
        .atten = ADC_ATTEN_DB_12,  // Use updated constant
        .bitwidth = MISCS_ADC_BITWIDTH,
    };
    ret = adc_cali_create_scheme_curve_fitting(&cali_config, &adc1_cali_handle);
    if (ret == ESP_OK) {
        adc_calibration_enable = true;
        ESP_LOGI(TAG, "ADC calibration enabled");
    } else {
        ESP_LOGE(TAG, "ADC calibration failed - accurate voltage readings require calibration");
        adc_oneshot_del_unit(adc1_handle);
        adc1_handle = NULL;
        return ret;
    }

    // Initialize rotary encoder
    ret = miscs_encoder_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize rotary encoder");
        return ret;
    }

    ESP_LOGI(TAG, "Miscellaneous peripheral hardware initialized successfully");
    return ESP_OK;
}

esp_err_t miscs_deinit(void)
{
    ESP_LOGI(TAG, "Deinitializing miscellaneous peripheral hardware");

    // Disable encoder interrupts
    gpio_isr_handler_remove(MISCS_ENCODER_A_GPIO);
    gpio_isr_handler_remove(MISCS_ENCODER_B_GPIO);

    // Deinitialize ADC calibration
    if (adc_calibration_enable && adc1_cali_handle) {
        adc_cali_delete_scheme_curve_fitting(adc1_cali_handle);
        adc1_cali_handle = NULL;
        adc_calibration_enable = false;
    }

    // Deinitialize ADC unit
    if (adc1_handle) {
        adc_oneshot_del_unit(adc1_handle);
        adc1_handle = NULL;
    }

    ESP_LOGI(TAG, "Miscellaneous peripheral hardware deinitialized successfully");
    return ESP_OK;
}

static esp_err_t miscs_gpio_config(gpio_num_t gpio_num, gpio_mode_t mode, gpio_pull_mode_t pull_mode)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << gpio_num),
        .mode = mode,
        .pull_up_en = (pull_mode == GPIO_PULLUP_ONLY || pull_mode == GPIO_PULLUP_PULLDOWN),
        .pull_down_en = (pull_mode == GPIO_PULLDOWN_ONLY || pull_mode == GPIO_PULLUP_PULLDOWN),
        .intr_type = GPIO_INTR_DISABLE,
    };

    esp_err_t ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure GPIO %d: %s", gpio_num, esp_err_to_name(ret));
    }

    return ret;
}

// USB Power Detection Functions
bool miscs_is_usb_powered(void)
{
    return gpio_get_level(MISCS_USB_POWER_GPIO) == 1;
}

// Battery Charge Status Functions
bool miscs_is_battery_charging(void)
{
    return gpio_get_level(MISCS_BATTERY_CHARGE_GPIO) == 0; // Low means charging
}

// Battery Voltage ADC Functions
static esp_err_t miscs_read_battery_voltage(uint32_t *voltage_mv)
{
    if (voltage_mv == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (adc1_handle == NULL) {
        ESP_LOGE(TAG, "ADC not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    // Take 5 samples and calculate average for better accuracy
    uint32_t voltage_sum = 0;
    esp_err_t ret = ESP_OK;

    // Check if calibration is available
    if (!adc_calibration_enable || !adc1_cali_handle) {
        ESP_LOGE(TAG, "ADC calibration not available");
        return ESP_ERR_NOT_SUPPORTED;
    }

    for (int i = 0; i < MISCS_ADC_READ_TIMES; i++) {
        int adc_raw = 0;
        ret = adc_oneshot_read(adc1_handle, ADC_CHANNEL_4, &adc_raw);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to read ADC sample %d: %s", i, esp_err_to_name(ret));
            return ret;
        }

        int voltage_cal = 0;
        ret = adc_cali_raw_to_voltage(adc1_cali_handle, adc_raw, &voltage_cal);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "ADC calibration failed for sample %d: %s", i, esp_err_to_name(ret));
            return ret;
        }

        voltage_sum += voltage_cal;

        // Small delay between samples to avoid sampling noise
        if (i < MISCS_ADC_READ_TIMES - 1) {
            vTaskDelay(pdMS_TO_TICKS(2)); // 2ms delay between samples
        }
    }

    // Calculate average voltage
    *voltage_mv = voltage_sum / MISCS_ADC_READ_TIMES;

    return ESP_OK;
}

esp_err_t miscs_get_battery_percentage(uint8_t *percentage)
{
    if (percentage == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    uint32_t voltage_mv = 0;
    esp_err_t ret = miscs_read_battery_voltage(&voltage_mv);
    if (ret != ESP_OK) {
        return ret;
    }

    // Typical Li-Ion battery voltage range:
    // Full charge: ~4.2V (4200mV)
    // Empty: ~3.0V (3000mV)
    // These values may need adjustment based on your specific battery

    const uint32_t BATTERY_MIN_VOLTAGE = 3000; // 3.0V
    const uint32_t BATTERY_MAX_VOLTAGE = 4200; // 4.2V

    if (voltage_mv >= BATTERY_MAX_VOLTAGE) {
        *percentage = 100;
    } else if (voltage_mv <= BATTERY_MIN_VOLTAGE) {
        *percentage = 0;
    } else {
        // Linear interpolation between min and max voltage
        uint32_t range = BATTERY_MAX_VOLTAGE - BATTERY_MIN_VOLTAGE;
        uint32_t offset = voltage_mv - BATTERY_MIN_VOLTAGE;
        *percentage = (uint8_t)((offset * 100) / range);
    }

    return ESP_OK;
}

// Rotary Encoder Functions
static esp_err_t miscs_encoder_init(void)
{
    esp_err_t ret = ESP_OK;

    // Configure encoder GPIO pins as inputs with pull-up
    ret = miscs_gpio_config(MISCS_ENCODER_A_GPIO, GPIO_MODE_INPUT, GPIO_PULLUP_ONLY);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure encoder A GPIO");
        return ret;
    }

    ret = miscs_gpio_config(MISCS_ENCODER_B_GPIO, GPIO_MODE_INPUT, GPIO_PULLUP_ONLY);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure encoder B GPIO");
        return ret;
    }

    // Configure encoder button GPIO as input with pull-up (button connects to GND)
    ret = miscs_gpio_config(MISCS_ENCODER_BTN_GPIO, GPIO_MODE_INPUT, GPIO_PULLUP_ONLY);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure encoder button GPIO");
        return ret;
    }

    // Install GPIO ISR service
    ret = gpio_install_isr_service(0);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Failed to install GPIO ISR service: %s", esp_err_to_name(ret));
        return ret;
    }

    // Add ISR handlers for both encoder pins
    ret = gpio_isr_handler_add(MISCS_ENCODER_A_GPIO, encoder_isr_handler, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add ISR handler for encoder A: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = gpio_isr_handler_add(MISCS_ENCODER_B_GPIO, encoder_isr_handler, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add ISR handler for encoder B: %s", esp_err_to_name(ret));
        gpio_isr_handler_remove(MISCS_ENCODER_A_GPIO);
        return ret;
    }

    // Enable interrupts on both edges for both pins
    ret = gpio_set_intr_type(MISCS_ENCODER_A_GPIO, GPIO_INTR_ANYEDGE);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set interrupt type for encoder A");
        return ret;
    }

    ret = gpio_set_intr_type(MISCS_ENCODER_B_GPIO, GPIO_INTR_ANYEDGE);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set interrupt type for encoder B");
        return ret;
    }

    // Initialize encoder state
    encoder_last_state = (gpio_get_level(MISCS_ENCODER_A_GPIO) << 1) | gpio_get_level(MISCS_ENCODER_B_GPIO);
    encoder_position = 0;
    encoder_raw_position = 0;
    encoder_direction = MISCS_ENCODER_STOPPED;

    ESP_LOGI(TAG, "Rotary encoder initialized successfully");
    return ESP_OK;
}

int32_t miscs_encoder_get_position(void)
{
    return encoder_position;
}

void miscs_encoder_reset_position(void)
{
    encoder_position = 0;
    encoder_raw_position = 0;
    encoder_direction = MISCS_ENCODER_STOPPED;
}

miscs_encoder_direction_t miscs_encoder_get_direction(void)
{
    return encoder_direction;
}

bool miscs_encoder_button_pressed(void)
{
    return gpio_get_level(MISCS_ENCODER_BTN_GPIO) == 0; // Button is pressed when GPIO is low (connected to GND)
}
