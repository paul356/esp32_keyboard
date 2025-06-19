/*
 * drv_loop.c - Generic event system implementation for MK32 keyboard
 * Pure event system without any domain-specific logic
 */

#include "drv_loop.h"
#include "esp_log.h"
#include "esp_event.h"

static const char* TAG = "drv_loop";

// Static variables
static esp_event_loop_handle_t s_event_loop = NULL;

esp_err_t drv_loop_init(void)
{
    if (s_event_loop) {
        ESP_LOGW(TAG, "Driver loop already initialized");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Initializing generic event system");

    // Create a dedicated event loop
    esp_event_loop_args_t loop_args = {
        .queue_size = 32,
        .task_name = "drv_loop_task",
        .task_priority = 10,
        .task_stack_size = 8192,
        .task_core_id = tskNO_AFFINITY
    };

    esp_err_t err = esp_event_loop_create(&loop_args, &s_event_loop);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create event loop: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "Generic event system initialized successfully");
    return ESP_OK;
}

esp_err_t drv_loop_deinit(void)
{

    // Delete the dedicated event loop
    if (s_event_loop) {
        esp_err_t err = esp_event_loop_delete(s_event_loop);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "Failed to delete event loop: %s", esp_err_to_name(err));
        }
        s_event_loop = NULL;
    }

    ESP_LOGI(TAG, "Generic event system deinitialized");
    return ESP_OK;
}

esp_err_t drv_loop_register_handler(esp_event_base_t event_base,
                                   int32_t event_id,
                                   esp_event_handler_t event_handler,
                                   void* event_handler_arg)
{
    if (!s_event_loop) {
        ESP_LOGE(TAG, "Event system not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (!event_base || !event_handler) {
        ESP_LOGE(TAG, "Invalid handler registration parameters");
        return ESP_ERR_INVALID_ARG;
    }

    return esp_event_handler_register_with(s_event_loop, event_base, event_id, event_handler, event_handler_arg);
}

esp_err_t drv_loop_unregister_handler(esp_event_base_t event_base,
                                     int32_t event_id,
                                     esp_event_handler_t event_handler)
{
    if (!s_event_loop) {
        ESP_LOGE(TAG, "Event system not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    return esp_event_handler_unregister_with(s_event_loop, event_base, event_id, event_handler);
}

esp_err_t drv_loop_post_event(esp_event_base_t event_base,
                             int32_t event_id,
                             const void* event_data,
                             size_t event_data_size,
                             TickType_t ticks_to_wait)
{
    if (!s_event_loop) {
        ESP_LOGE(TAG, "Event system not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (!event_base) {
        ESP_LOGE(TAG, "Invalid event base");
        return ESP_ERR_INVALID_ARG;
    }

    return esp_event_post_to(s_event_loop, event_base, event_id, event_data, event_data_size, ticks_to_wait);
}

esp_err_t drv_loop_post_event_isr(esp_event_base_t event_base,
                                  int32_t event_id,
                                  const void *event_data,
                                  size_t event_data_size)
{
    if (!s_event_loop) {
        return ESP_ERR_INVALID_STATE;
    }

    if (!event_base) {
        return ESP_ERR_INVALID_ARG;
    }

    return esp_event_post_to(s_event_loop, event_base, event_id, event_data, event_data_size, 0);
}

esp_event_loop_handle_t drv_loop_get_handle(void)
{
    return s_event_loop;
}
