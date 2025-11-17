#include "led_ctrl.h"
#include "drv_loop.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include <string.h>
#include <math.h>

/**
 * LED control event IDs
 */
typedef enum {
    LED_CTRL_EVENT_KEYSTROKE,  // Keystroke event received
    LED_CTRL_EVENT_CLEAR_LEDS,  // Clear all LEDs event
    LED_CTRL_EVENT_UPDATE_LEDS,  // Update/refresh LEDs event
    LED_CTRL_EVENT_ENABLE_RMT,  // Enable RMT hardware
    LED_CTRL_EVENT_DISABLE_RMT,  // Disable RMT hardware
    LED_CTRL_EVENT_MAX
} led_ctrl_event_id_t;

typedef struct {
    uint8_t row;
    uint8_t col;
    bool pressed;  // True if key is pressed, false if released
} led_ctrl_keystroke_t;

/**
 * Pattern configuration structure
 */
typedef struct {
    led_pattern_type_e pattern;     // Pattern type
    led_drv_color_t primary_color;  // Primary color for pattern
    led_drv_color_t secondary_color; // Secondary color (for gradients, etc.)
    uint8_t brightness;             // Overall brightness (0-255)
    uint32_t param1;                // Pattern-specific parameter 1
    uint32_t param2;                // Pattern-specific parameter 2
} led_pattern_config_t;

// Component state
static bool s_initialized = false;
static bool s_rmt_enabled = false;  // Track RMT hardware state (initially enabled)

static const char *TAG = "led_ctrl";

// Event base definition
ESP_EVENT_DEFINE_BASE(LED_CTRL_EVENTS);

// Current pattern configuration
static led_pattern_config_t s_current_pattern = {
    .pattern = LED_PATTERN_OFF,
    .primary_color = LED_COLOR_BLACK,
    .secondary_color = LED_COLOR_BLACK,
    .brightness = 128,
    .param1 = 5,
    .param2 = 0
};

// Forward declarations
static void led_ctrl_event_handler(void *event_handler_arg, esp_event_base_t event_base,
                                   int32_t event_id, void *event_data);
static void update_led_pattern(uint32_t frame);
static void refresh_led_pattern(void);

esp_err_t led_ctrl_init(void) {
    if (s_initialized) {
        ESP_LOGW(TAG, "LED control already initialized");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Initializing LED control component");

    // Initialize LED driver first
    esp_err_t ret = led_drv_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize LED driver: %s", esp_err_to_name(ret));
        return ret;
    }
    s_rmt_enabled = true;

    // Register event handler with drv_loop
    ret = drv_loop_register_handler(LED_CTRL_EVENTS, ESP_EVENT_ANY_ID,
                                   led_ctrl_event_handler, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register event handler: %s", esp_err_to_name(ret));
        return ret;
    }

    // Initialize all LEDs to off
    led_drv_clear();

    s_initialized = true;
    ESP_LOGI(TAG, "LED control component initialized successfully");

    drv_loop_post_event(LED_CTRL_EVENTS, LED_CTRL_EVENT_CLEAR_LEDS, NULL, 0, 0);

    return ESP_OK;
}

esp_err_t led_ctrl_set_pattern(led_pattern_type_e pattern_type, uint32_t param1, uint32_t param2) {
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    s_current_pattern.pattern = pattern_type;
    s_current_pattern.param1 = param1;
    s_current_pattern.param2 = param2;

    ESP_LOGI(TAG, "LED pattern set to %d with param1=%lu, param2=%lu", pattern_type, param1, param2);
    return ESP_OK;
}

esp_err_t led_ctrl_get_pattern(led_pattern_type_e *pattern_type, uint32_t *param1, uint32_t *param2) {
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (pattern_type == NULL || param1 == NULL || param2 == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    // Copy current pattern configuration to provided pointers
    *pattern_type = s_current_pattern.pattern;
    *param1 = s_current_pattern.param1;
    *param2 = s_current_pattern.param2;

    return ESP_OK;
}

esp_err_t led_ctrl_keystroke(uint8_t row, uint8_t col, bool pressed) {
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (!pressed)
    {
        // only handle key press events now
        return ESP_OK;
    }

    if (s_current_pattern.pattern == LED_PATTERN_OFF) {
        // If the current pattern is OFF, we can just ignore the keystroke
        return ESP_OK;
    }

    ESP_LOGD(TAG, "Keystroke: pos=(%d,%d), pressed=%d", row, col, pressed);

    // Fill event structure and post to drv_loop
    led_ctrl_keystroke_t keystroke = {
        .row = row,
        .col = col,
        .pressed = pressed,
    };

    return drv_loop_post_event(LED_CTRL_EVENTS, LED_CTRL_EVENT_KEYSTROKE,
                              &keystroke, sizeof(keystroke), 0);
}

/**
 * @brief Apply brightness scaling to a color
 */
static led_drv_color_t apply_brightness(led_drv_color_t color, uint8_t brightness) {
    led_drv_color_t result;
    result.red = (color.red * brightness) >> 8;
    result.green = (color.green * brightness) >> 8;
    result.blue = (color.blue * brightness) >> 8;
    return result;
}

static void draw_hit_key_pattern(uint32_t index)
{
    uint32_t prev = index - 1;
    // Example: Increment a counter for demonstration purposes
    if (prev >= LED_DRV_NUM_LEDS) {
        prev = LED_DRV_NUM_LEDS - 1;
    }

    // Apply brightness to colors
    led_drv_color_t blue_brightness = apply_brightness(LED_COLOR_BLUE, s_current_pattern.brightness);

    // Update LEDs with brightness-adjusted colors
    led_drv_set_led(prev, LED_COLOR_BLACK);
    led_drv_set_led(index, blue_brightness); // Set the LED at index 'count' to blue

    // Only update LED strip if RMT hardware is enabled
    if (s_rmt_enabled) {
        led_drv_update(); // Update the LED strip with the new colors
    } else {
        ESP_LOGD(TAG, "Skipping LED update - RMT disabled");
    }
}

/**
 * @brief Update LED pattern based on keystroke
 * This function updates the LED pattern based on the current pattern configuration
 */
static uint32_t current_frame;
static void update_led_pattern(uint32_t frame) {
    ESP_LOGD(TAG, "Updating LED pattern: frame=%lu", frame);

    current_frame = frame;
    uint32_t index = frame % LED_DRV_NUM_LEDS;
    draw_hit_key_pattern(index);

    ESP_LOGD(TAG, "Keystroke count: %lu", frame);
}

/**
 * @brief Refresh the current LED pattern
 * This function refreshes all LEDs with the current brightness setting
 */
static void refresh_led_pattern(void) {
    ESP_LOGI(TAG, "Refreshing LED pattern with brightness=%d", s_current_pattern.brightness);

    // Only refresh if RMT is enabled
    if (!s_rmt_enabled) {
        ESP_LOGD(TAG, "Skipping pattern refresh - RMT disabled");
        return;
    }

    uint32_t index = current_frame % LED_DRV_NUM_LEDS;
    draw_hit_key_pattern(index);
}

static void clear_all_leds(void)
{
    led_drv_clear();
    // Only update if RMT is enabled
    if (s_rmt_enabled)
    {
        led_drv_update();
    }
}

static uint32_t s_count = 0;
static void handle_keystroke_event(uint8_t row, uint8_t col, bool pressed) {
    // This function handles keystroke events by delegating to update_led_pattern
    ESP_LOGD(TAG, "Keystroke event: row=%d, col=%d, pressed=%d", row, col, pressed);

    // Update the LED pattern based on current configuration
    update_led_pattern(s_count);
    ++s_count;
}

esp_err_t led_ctrl_set_brightness(uint8_t brightness) {
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (brightness > 100) {
        ESP_LOGE(TAG, "Brightness %d out of range (0-100)", brightness);
        return ESP_ERR_INVALID_ARG;
    }

    // Convert from 0-100 range to 0-255 range for internal use
    s_current_pattern.brightness = (brightness * 255) / 100;

    ESP_LOGI(TAG, "LED brightness set to %u%%", brightness);

    // Trigger a pattern refresh
    drv_loop_post_event(LED_CTRL_EVENTS, LED_CTRL_EVENT_UPDATE_LEDS, NULL, 0, 0);

    return ESP_OK;
}

esp_err_t led_ctrl_get_brightness(uint8_t *brightness) {
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (brightness == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    // Convert from 0-255 range to 0-100 range for external use
    *brightness = (s_current_pattern.brightness * 100) / 255;

    return ESP_OK;
}

esp_err_t led_ctrl_clear(void) {
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "Clearing all LEDs");

    // Trigger a pattern refresh if needed
    drv_loop_post_event(LED_CTRL_EVENTS, LED_CTRL_EVENT_CLEAR_LEDS, NULL, 0, 0);

    return ESP_OK;
}

esp_err_t led_ctrl_enable_rmt(void) {
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "Requesting RMT enable via event");

    // Post event to enable RMT in drv_loop context
    return drv_loop_post_event(LED_CTRL_EVENTS, LED_CTRL_EVENT_ENABLE_RMT, NULL, 0, 0);
}

esp_err_t led_ctrl_disable_rmt(void) {
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "Requesting RMT disable via event");

    // Post event to disable RMT in drv_loop context
    return drv_loop_post_event(LED_CTRL_EVENTS, LED_CTRL_EVENT_DISABLE_RMT, NULL, 0, 0);
}

// Main event handler
static void led_ctrl_event_handler(void *event_handler_arg, esp_event_base_t event_base,
                                   int32_t event_id, void *event_data) {
    if (event_base != LED_CTRL_EVENTS) {
        return;
    }

    switch (event_id) {
        case LED_CTRL_EVENT_KEYSTROKE: {
            if (event_data) {
                led_ctrl_keystroke_t *keystroke = (led_ctrl_keystroke_t *)event_data;
                ESP_LOGD(TAG, "Processing keystroke: pos=(%d,%d), pressed=%d",
                        keystroke->row, keystroke->col, keystroke->pressed);

                // Handle keystroke-based patterns if needed
                handle_keystroke_event(keystroke->row, keystroke->col, keystroke->pressed);
            }
            break;
        }

        case LED_CTRL_EVENT_CLEAR_LEDS: {
            clear_all_leds();
            ESP_LOGI(TAG, "Clearing all LEDs");
            break;
        }

        case LED_CTRL_EVENT_UPDATE_LEDS: {
            refresh_led_pattern();
            ESP_LOGI(TAG, "Refreshing LED pattern");
            break;
        }

        case LED_CTRL_EVENT_ENABLE_RMT: {
            if (!s_rmt_enabled) {
                esp_err_t ret = led_drv_enable();
                if (ret == ESP_OK) {
                    s_rmt_enabled = true;
                    ESP_LOGI(TAG, "RMT hardware enabled");
                } else {
                    ESP_LOGE(TAG, "Failed to enable RMT hardware: %s", esp_err_to_name(ret));
                }
            } else {
                ESP_LOGD(TAG, "RMT hardware already enabled");
            }
            break;
        }

        case LED_CTRL_EVENT_DISABLE_RMT: {
            if (s_rmt_enabled) {
                esp_err_t ret = led_drv_disable();
                if (ret == ESP_OK) {
                    s_rmt_enabled = false;
                    ESP_LOGI(TAG, "RMT hardware disabled");
                } else {
                    ESP_LOGE(TAG, "Failed to disable RMT hardware: %s", esp_err_to_name(ret));
                }
            } else {
                ESP_LOGD(TAG, "RMT hardware already disabled");
            }
            break;
        }

        default:
            ESP_LOGW(TAG, "Unknown event ID: %ld", event_id);
            break;
    }
}
