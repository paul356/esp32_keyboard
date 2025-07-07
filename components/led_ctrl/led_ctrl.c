#include "led_ctrl.h"
#include "drv_loop.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include <string.h>
#include <math.h>

#define MAX_LEDS 70

/**
 * LED control event IDs
 */
typedef enum {
    LED_CTRL_EVENT_KEYSTROKE,  // Keystroke event received
    LED_CTRL_EVENT_CLEAR_LEDS,  // Pattern update event
    LED_CTRL_EVENT_MAX
} led_ctrl_event_id_t;

typedef struct {
    uint8_t row;
    uint8_t col;
    bool pressed;  // True if key is pressed, false if released
} led_ctrl_keystroke_t;

// Component state
static bool s_initialized = false;

static const char *TAG = "led_ctrl";

// Event base definition
ESP_EVENT_DEFINE_BASE(LED_CTRL_EVENTS);

// Current pattern configuration
static led_pattern_config_t s_current_pattern = {
    .pattern = LED_PATTERN_OFF,
    .primary_color = {0, 0, 0},
    .secondary_color = {0, 0, 0},
    .brightness = 128,
    .param1 = 5,
    .param2 = 0
};

// Forward declarations
static void led_ctrl_event_handler(void *event_handler_arg, esp_event_base_t event_base,
                                   int32_t event_id, void *event_data);

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
    
    return ESP_OK;
}

esp_err_t led_ctrl_set_pattern(led_pattern_type_t pattern_type, uint32_t param1, uint32_t param2) {
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    s_current_pattern.pattern = pattern_type;
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


// Timer callback function to clear LEDs
/*static void clear_leds_timer_callback(void* arg) {
    ESP_LOGD(TAG, "Timer callback: posting clear LED event");
    drv_loop_post_event(LED_CTRL_EVENTS, LED_CTRL_EVENT_CLEAR_LEDS, NULL, 0, 0);
}*/

static int count = 0;
static void handle_keystroke_event(uint8_t row, uint8_t col, bool pressed) {
    // This function can be used to handle keystroke events
    // For now, we just log the event
    ESP_LOGI(TAG, "Keystroke event: row=%d, col=%d, pressed=%d", row, col, pressed);

    count ++;
    // Example: Increment a counter for demonstration purposes
    if (count >= MAX_LEDS) {
        ESP_LOGI(TAG, "Maximum keystroke count reached, resetting to 0");
        count = 0; // Reset count if it exceeds MAX_LEDS
        led_drv_clear(); // Clear all LEDs
    }

    led_drv_color_t white = { 255, 255, 255 };
    led_drv_set_led(count, white); // Set the LED at index 'count' to red

    led_drv_update(); // Update the LED strip with the new colors

    // Create a one-shot timer to clear LEDs after 5ms
    /*esp_timer_handle_t clear_timer;
    const esp_timer_create_args_t clear_timer_args = {
        .callback = &clear_leds_timer_callback,
        .arg = NULL,
        .name = "clear_leds_timer"
    };
    
    esp_err_t timer_ret = esp_timer_create(&clear_timer_args, &clear_timer);
    if (timer_ret == ESP_OK) {
        esp_timer_start_once(clear_timer, s_current_pattern.param1 * 1000); // 5ms in microseconds
        ESP_LOGD(TAG, "Clear LEDs timer started (5ms)");
    } else {
        ESP_LOGE(TAG, "Failed to create clear LEDs timer: %s", esp_err_to_name(timer_ret));
    }*/
    
    ESP_LOGI(TAG, "Keystroke count: %d", count);
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
            led_drv_clear();
            led_drv_update();
            ESP_LOGI(TAG, "Clearing all LEDs");
            break;
        }
        
        default:
            ESP_LOGW(TAG, "Unknown event ID: %ld", event_id);
            break;
    }
}
