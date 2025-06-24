#include "led_drv.h"
#include "esp_log.h"
#include "esp_check.h"
#include "driver/rmt_tx.h"
#include "driver/rmt_encoder.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

/**
 * WS2812 Timing Configuration (in nanoseconds)
 * These values are based on WS2812 datasheet specifications
 */
#define LED_DRV_RMT_RESOLUTION_HZ   10000000  // 10MHz resolution, 1 tick = 0.1us
#define LED_DRV_WS2812_T0H_NS       300       // 0 code, high level time
#define LED_DRV_WS2812_T0L_NS       900       // 0 code, low level time  
#define LED_DRV_WS2812_T1H_NS       600       // 1 code, high level time
#define LED_DRV_WS2812_T1L_NS       600       // 1 code, low level time
#define LED_DRV_WS2812_RESET_US     280       // Reset code duration in microseconds

static const char *TAG = "LED_DRV";

// Static variables
static rmt_channel_handle_t led_chan = NULL;
static rmt_encoder_handle_t led_encoder = NULL;
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
    
    // different led strip might have its own timing requirements, following parameter is for WS2812
    rmt_bytes_encoder_config_t bytes_encoder_config = {
        .bit0 = {
            .level0 = 1,
            .duration0 = 0.3 * config->resolution / 1000000, // T0H=0.3us
            .level1 = 0,
            .duration1 = 0.9 * config->resolution / 1000000, // T0L=0.9us
        },
        .bit1 = {
            .level0 = 1,
            .duration0 = 0.6 * config->resolution / 1000000, // T1H=0.6us
            .level1 = 0,
            .duration1 = 0.6 * config->resolution / 1000000, // T1L=0.6us
        },
        .flags.msb_first = 1 // WS2812 transfer bit order: G7...G0R7...R0B7...B0
    };
    ESP_GOTO_ON_ERROR(rmt_new_bytes_encoder(&bytes_encoder_config, &led_encoder->bytes_encoder), err, TAG, "create bytes encoder failed");
    
    rmt_copy_encoder_config_t copy_encoder_config = {};
    ESP_GOTO_ON_ERROR(rmt_new_copy_encoder(&copy_encoder_config, &led_encoder->copy_encoder), err, TAG, "create copy encoder failed");
    
    uint32_t reset_ticks = config->resolution / 1000000 * 50 / 2; // reset code duration defaults to 50us
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
    ESP_LOGI(TAG, "Initializing LED strip driver (GPIO%d, %d LEDs)", LED_DRV_GPIO_PIN, LED_DRV_NUM_LEDS);
    
    esp_err_t ret = ESP_OK;
    
    // Create RMT TX channel
    rmt_tx_channel_config_t tx_chan_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .gpio_num = LED_DRV_GPIO_PIN,
        .mem_block_symbols = 64, // increase from default 64
        .resolution_hz = LED_DRV_RMT_RESOLUTION_HZ,
        .trans_queue_depth = 4, // set the number of transactions that can be pending in the background
    };
    ret = rmt_new_tx_channel(&tx_chan_config, &led_chan);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create RMT TX channel: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Create LED strip encoder
    led_strip_encoder_config_t encoder_config = {
        .resolution = LED_DRV_RMT_RESOLUTION_HZ,
    };
    ret = rmt_new_led_strip_encoder(&encoder_config, &led_encoder);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create LED strip encoder: %s", esp_err_to_name(ret));
        rmt_del_channel(led_chan);
        led_chan = NULL;
        return ret;
    }
    
    // Enable RMT TX channel
    ret = rmt_enable(led_chan);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to enable RMT channel: %s", esp_err_to_name(ret));
        rmt_del_encoder(led_encoder);
        rmt_del_channel(led_chan);
        led_chan = NULL;
        led_encoder = NULL;
        return ret;
    }
    
    // Initialize LED buffer to all black (off)
    memset(led_buffer, 0, sizeof(led_buffer));
    
    // Clear the strip initially
    led_drv_clear();
    
    ESP_LOGI(TAG, "LED strip driver initialized successfully");
    return ESP_OK;
}

esp_err_t led_drv_deinit(void)
{
    ESP_LOGI(TAG, "Deinitializing LED strip driver");
    
    // Clear the strip before deinitializing
    if (led_chan && led_encoder) {
        led_drv_clear();
        vTaskDelay(pdMS_TO_TICKS(50)); // Give time for the clear operation to complete
    }
    
    // Disable and delete RMT channel
    if (led_chan) {
        rmt_disable(led_chan);
        rmt_del_channel(led_chan);
        led_chan = NULL;
    }
    
    // Delete encoder
    if (led_encoder) {
        rmt_del_encoder(led_encoder);
        led_encoder = NULL;
    }
    
    ESP_LOGI(TAG, "LED strip driver deinitialized successfully");
    return ESP_OK;
}

static esp_err_t led_drv_write(const led_drv_color_t *led_data, uint32_t num_leds)
{
    if (led_data == NULL || num_leds == 0) {
        ESP_LOGE(TAG, "LED data pointer is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    
    if (led_chan == NULL || led_encoder == NULL) {
        ESP_LOGE(TAG, "LED driver not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    // Convert RGB to GRB format (WS2812 expects GRB)
    uint8_t *led_strip_pixels = (uint8_t *)malloc(num_leds * 3);
    for (int i = 0; i < num_leds; i++) {
        led_strip_pixels[i * 3 + 0] = led_data[i].green;  // G
        led_strip_pixels[i * 3 + 1] = led_data[i].red;    // R
        led_strip_pixels[i * 3 + 2] = led_data[i].blue;   // B
    }
    
    // Transmit the data
    esp_err_t ret = rmt_transmit(led_chan, led_encoder, led_strip_pixels, num_leds * 3, &tx_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to transmit LED data: %s", esp_err_to_name(ret));
        goto cleanup;
    }
    
    // Wait for transmission to complete
    ret = rmt_tx_wait_all_done(led_chan, portMAX_DELAY);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to wait for transmission completion: %s", esp_err_to_name(ret));
        goto cleanup;
    }
    
cleanup:
    free(led_strip_pixels);
    return ret;
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
    return led_drv_write(led_buffer, LED_DRV_NUM_LEDS);
}
