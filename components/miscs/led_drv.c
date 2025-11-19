#include "led_drv.h"
#include "esp_log.h"
#include "esp_check.h"
#include "driver/rmt_tx.h"
#include "driver/rmt_encoder.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

/**
 * WS2812 Timing Configuration
 */
#define LED_DRV_RMT_RESOLUTION_HZ   4000000   // 4MHz resolution, 1 tick = 0.25us

static const char *TAG = "LED_DRV";

// GPIO pins for each LED strip
static const uint8_t strip_gpio_pins[LED_DRV_NUM_STRIPS] = {
    LED_DRV_STRIP1_GPIO,
    LED_DRV_STRIP2_GPIO,
    LED_DRV_STRIP3_GPIO
};

// LED count for each strip
static const uint8_t strip_led_counts[LED_DRV_NUM_STRIPS] = {
    LED_DRV_STRIP1_END - LED_DRV_STRIP1_START + 1,  // 20 LEDs
    LED_DRV_STRIP2_END - LED_DRV_STRIP2_START + 1,  // 21 LEDs
    LED_DRV_STRIP3_END - LED_DRV_STRIP3_START + 1   // 20 LEDs
};

// Static variables - now arrays for multiple strips
static rmt_channel_handle_t led_channels[LED_DRV_NUM_STRIPS] = {NULL};
static rmt_encoder_handle_t led_encoders[LED_DRV_NUM_STRIPS] = {NULL};
static rmt_transmit_config_t tx_config = {
    .loop_count = 0, // no transfer loop
};

// LED buffer for set_led/update operations
static led_drv_color_t led_buffer[LED_DRV_NUM_LEDS];

/**
 * LED Strip Encoder Implementation
 * This encoder converts RGB data to RMT symbols for WS2812 LEDs
 */
typedef struct {
    rmt_encoder_t base;
    rmt_encoder_t *bytes_encoder;
    rmt_encoder_t *copy_encoder;
    int state;
    rmt_symbol_word_t reset_code;
} rmt_led_strip_encoder_t;

typedef struct {
    uint32_t resolution; ///< Encoder resolution, in Hz
} led_strip_encoder_config_t;

static size_t rmt_encode_led_strip(rmt_encoder_t *encoder, rmt_channel_handle_t channel,
                                   const void *primary_data, size_t data_size, rmt_encode_state_t *ret_state)
{
    rmt_led_strip_encoder_t *led_encoder = __containerof(encoder, rmt_led_strip_encoder_t, base);
    rmt_encoder_handle_t bytes_encoder = led_encoder->bytes_encoder;
    rmt_encoder_handle_t copy_encoder = led_encoder->copy_encoder;
    rmt_encode_state_t session_state = 0;
    rmt_encode_state_t state = 0;
    size_t encoded_symbols = 0;

    switch (led_encoder->state) {
    case 0: // send RGB data
        encoded_symbols += bytes_encoder->encode(bytes_encoder, channel, primary_data, data_size, &session_state);
        if (session_state & RMT_ENCODING_COMPLETE) {
            led_encoder->state = 1; // switch to next state when current encoding session finished
        }
        if (session_state & RMT_ENCODING_MEM_FULL) {
            state |= RMT_ENCODING_MEM_FULL;
            goto out; // yield if there's no free space for encoding artifacts
        }
    // fall-through
    case 1: // send reset code
        encoded_symbols += copy_encoder->encode(copy_encoder, channel, &led_encoder->reset_code,
                                                sizeof(led_encoder->reset_code), &session_state);
        if (session_state & RMT_ENCODING_COMPLETE) {
            led_encoder->state = 0; // back to the initial encoding session
            state |= RMT_ENCODING_COMPLETE;
        }
        if (session_state & RMT_ENCODING_MEM_FULL) {
            state |= RMT_ENCODING_MEM_FULL;
            goto out; // yield if there's no free space for encoding artifacts
        }
    }
out:
    *ret_state = state;
    return encoded_symbols;
}

static esp_err_t rmt_del_led_strip_encoder(rmt_encoder_t *encoder)
{
    rmt_led_strip_encoder_t *led_encoder = __containerof(encoder, rmt_led_strip_encoder_t, base);
    rmt_del_encoder(led_encoder->bytes_encoder);
    rmt_del_encoder(led_encoder->copy_encoder);
    free(led_encoder);
    return ESP_OK;
}

static esp_err_t rmt_led_strip_encoder_reset(rmt_encoder_t *encoder)
{
    rmt_led_strip_encoder_t *led_encoder = __containerof(encoder, rmt_led_strip_encoder_t, base);
    rmt_encoder_reset(led_encoder->bytes_encoder);
    rmt_encoder_reset(led_encoder->copy_encoder);
    led_encoder->state = 0;
    return ESP_OK;
}

static esp_err_t rmt_new_led_strip_encoder(const led_strip_encoder_config_t *config, rmt_encoder_handle_t *ret_encoder)
{
    esp_err_t ret = ESP_OK;
    rmt_led_strip_encoder_t *led_encoder = NULL;

    ESP_RETURN_ON_FALSE(config && ret_encoder, ESP_ERR_INVALID_ARG, TAG, "invalid argument");
    led_encoder = calloc(1, sizeof(rmt_led_strip_encoder_t));
    ESP_RETURN_ON_FALSE(led_encoder, ESP_ERR_NO_MEM, TAG, "no mem for led strip encoder");

    led_encoder->base.encode = rmt_encode_led_strip;
    led_encoder->base.del = rmt_del_led_strip_encoder;
    led_encoder->base.reset = rmt_led_strip_encoder_reset;

    // WS2812 timing requirements - calculated for 4MHz resolution (0.25us per tick)
    rmt_bytes_encoder_config_t bytes_encoder_config = {
        .bit0 = {
            .level0 = 1,
            .duration0 = 1,  // T0H = 1 tick = 0.25us (within 0.2-0.5us range)
            .level1 = 0,
            .duration1 = 3,  // T0L = 3 ticks = 0.75us (within 0.65-1.0us range)
        },
        .bit1 = {
            .level0 = 1,
            .duration0 = 3,  // T1H = 3 ticks = 0.75us (within 0.65-1.0us range)
            .level1 = 0,
            .duration1 = 1,  // T1L = 1 tick = 0.25us (within 0.2-0.5us range)
        },
        .flags.msb_first = 1 // WS2812 transfer bit order: G7...G0R7...R0B7...B0
    };
    ESP_GOTO_ON_ERROR(rmt_new_bytes_encoder(&bytes_encoder_config, &led_encoder->bytes_encoder), err, TAG, "create bytes encoder failed");

    rmt_copy_encoder_config_t copy_encoder_config = {};
    ESP_GOTO_ON_ERROR(rmt_new_copy_encoder(&copy_encoder_config, &led_encoder->copy_encoder), err, TAG, "create copy encoder failed");

    uint32_t reset_ticks = 320; // 320 ticks = 80us reset period at 4MHz (320 * 0.25us = 80us)
    led_encoder->reset_code = (rmt_symbol_word_t) {
        .level0 = 0, .duration0 = reset_ticks,
        .level1 = 0, .duration1 = reset_ticks,
    };

    *ret_encoder = &led_encoder->base;
    return ESP_OK;

err:
    if (led_encoder) {
        if (led_encoder->bytes_encoder) {
            rmt_del_encoder(led_encoder->bytes_encoder);
        }
        if (led_encoder->copy_encoder) {
            rmt_del_encoder(led_encoder->copy_encoder);
        }
        free(led_encoder);
    }
    return ret;
}

esp_err_t led_drv_init(void)
{
    ESP_LOGI(TAG, "Initializing LED strip driver (%d strips, %d total LEDs)", LED_DRV_NUM_STRIPS, LED_DRV_NUM_LEDS);

    esp_err_t ret = ESP_OK;

    // Initialize each LED strip
    for (int strip = 0; strip < LED_DRV_NUM_STRIPS; strip++) {
        ESP_LOGI(TAG, "Initializing strip %d on GPIO%d with %d LEDs",
                 strip, strip_gpio_pins[strip], strip_led_counts[strip]);

        // Create RMT TX channel for this strip
        rmt_tx_channel_config_t tx_chan_config = {
            .clk_src = RMT_CLK_SRC_DEFAULT,
            .gpio_num = strip_gpio_pins[strip],
            .mem_block_symbols = SOC_RMT_MEM_WORDS_PER_CHANNEL,
            .resolution_hz = LED_DRV_RMT_RESOLUTION_HZ,
            .trans_queue_depth = 4
        };
        ret = rmt_new_tx_channel(&tx_chan_config, &led_channels[strip]);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to create RMT TX channel for strip %d: %s", strip, esp_err_to_name(ret));
            goto cleanup;
        }

        // Create LED strip encoder for this strip
        led_strip_encoder_config_t encoder_config = {
            .resolution = LED_DRV_RMT_RESOLUTION_HZ,
        };
        ret = rmt_new_led_strip_encoder(&encoder_config, &led_encoders[strip]);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to create LED strip encoder for strip %d: %s", strip, esp_err_to_name(ret));
            goto cleanup;
        }

        // Enable RMT TX channel
        ret = rmt_enable(led_channels[strip]);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to enable RMT channel for strip %d: %s", strip, esp_err_to_name(ret));
            goto cleanup;
        }
    }

    // Initialize LED buffer to all black (off)
    memset(led_buffer, 0, sizeof(led_buffer));

    // Clear all strips initially
    led_drv_clear();

    ESP_LOGI(TAG, "LED strip driver initialized successfully");
    return ESP_OK;

cleanup:
    // Clean up any initialized strips on error
    for (int i = 0; i < LED_DRV_NUM_STRIPS; i++) {
        if (led_channels[i]) {
            rmt_disable(led_channels[i]);
            rmt_del_channel(led_channels[i]);
            led_channels[i] = NULL;
        }
        if (led_encoders[i]) {
            rmt_del_encoder(led_encoders[i]);
            led_encoders[i] = NULL;
        }
    }
    return ret;
}

esp_err_t led_drv_deinit(void)
{
    ESP_LOGI(TAG, "Deinitializing LED strip driver");

    // Clear all strips before deinitializing
    for (int strip = 0; strip < LED_DRV_NUM_STRIPS; strip++) {
        if (led_channels[strip] && led_encoders[strip]) {
            // Clear this strip
            led_drv_color_t black = {0, 0, 0}; // GRB format: all zeros
            for (int i = 0; i < strip_led_counts[strip]; i++) {
                led_buffer[strip * strip_led_counts[0] + i] = black; // Simplified indexing
            }
        }
    }

    // Send clear command to all strips
    led_drv_update();
    vTaskDelay(pdMS_TO_TICKS(50)); // Give time for the clear operation to complete

    // Disable and delete all RMT channels and encoders
    for (int strip = 0; strip < LED_DRV_NUM_STRIPS; strip++) {
        if (led_channels[strip]) {
            rmt_disable(led_channels[strip]);
            rmt_del_channel(led_channels[strip]);
            led_channels[strip] = NULL;
        }

        if (led_encoders[strip]) {
            rmt_del_encoder(led_encoders[strip]);
            led_encoders[strip] = NULL;
        }
    }

    ESP_LOGI(TAG, "LED strip driver deinitialized successfully");
    return ESP_OK;
}

static esp_err_t led_drv_write(void)
{
    esp_err_t ret = ESP_OK;

    // Define strip start indices for direct buffer access
    // Since LED indices are contiguous for each strip, we can use
    // direct slices of led_buffer without allocation or copying
    const uint16_t strip_starts[LED_DRV_NUM_STRIPS] = {
        LED_DRV_STRIP1_START,
        LED_DRV_STRIP2_START,
        LED_DRV_STRIP3_START
    };

    // Process each strip separately
    for (int strip = 0; strip < LED_DRV_NUM_STRIPS; strip++) {
        if (led_channels[strip] == NULL || led_encoders[strip] == NULL) {
            ESP_LOGE(TAG, "LED strip %d not initialized", strip);
            return ESP_ERR_INVALID_STATE;
        }

        // Use direct slice of led_buffer (already in GRB format)
        uint8_t *strip_data = (uint8_t *)&led_buffer[strip_starts[strip]];
        size_t strip_data_size = strip_led_counts[strip] * 3; // 3 bytes per LED (GRB)

        // Transmit data to this strip (no allocation or copying needed)
        ret = rmt_transmit(led_channels[strip], led_encoders[strip],
                         strip_data, strip_data_size, &tx_config);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to transmit data to strip %d: %s", strip, esp_err_to_name(ret));
            return ret;
        }

        // Wait for transmission to complete
        ret = rmt_tx_wait_all_done(led_channels[strip], portMAX_DELAY);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to wait for transmission completion on strip %d: %s",
                     strip, esp_err_to_name(ret));
            return ret;
        }
    }

    // Add small delay for signal stability
    vTaskDelay(pdMS_TO_TICKS(1));
    return ESP_OK;
}

void led_drv_clear(void)
{
    memset(led_buffer, 0, sizeof(led_buffer)); // Set all LEDs to black (off)
}

esp_err_t led_drv_set_led(uint16_t index, led_drv_color_t color)
{
    if (index >= LED_DRV_NUM_LEDS) {
        ESP_LOGE(TAG, "LED index %d out of range (0-%d)", index, LED_DRV_NUM_LEDS - 1);
        return ESP_ERR_INVALID_ARG;
    }

    led_buffer[index] = color;
    return ESP_OK;
}

esp_err_t led_drv_update(void)
{
    return led_drv_write();
}

esp_err_t led_drv_enable(void)
{
    ESP_LOGI(TAG, "Enabling LED RMT peripheral");

    esp_err_t ret = ESP_OK;
    for (int strip = 0; strip < LED_DRV_NUM_STRIPS; strip++) {
        if (led_channels[strip] == NULL) {
            ESP_LOGW(TAG, "LED strip %d not initialized, skipping enable", strip);
            continue;
        }

        ret = rmt_enable(led_channels[strip]);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to enable RMT channel for strip %d: %s",
                     strip, esp_err_to_name(ret));
            return ret;
        }
    }

    ESP_LOGI(TAG, "LED RMT peripheral enabled successfully");
    return ESP_OK;
}

esp_err_t led_drv_disable(void)
{
    ESP_LOGI(TAG, "Disabling LED RMT peripheral to save power");

    esp_err_t ret = ESP_OK;
    for (int strip = 0; strip < LED_DRV_NUM_STRIPS; strip++) {
        if (led_channels[strip] == NULL) {
            ESP_LOGW(TAG, "LED strip %d not initialized, skipping disable", strip);
            continue;
        }

        ret = rmt_disable(led_channels[strip]);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to disable RMT channel for strip %d: %s",
                     strip, esp_err_to_name(ret));
            return ret;
        }
    }

    ESP_LOGI(TAG, "LED RMT peripheral disabled successfully");
    return ESP_OK;
}
