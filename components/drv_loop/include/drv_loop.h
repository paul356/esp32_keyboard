/*
 * drv_loop.h - Generic event system for MK32 keyboard
 *
 * This component provides a generic event system that components can use
 * to register their own events and handlers. No domain-specific logic.
 */

#ifndef DRV_LOOP_H
#define DRV_LOOP_H

#include "esp_event.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the generic event system
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t drv_loop_init(void);

/**
 * @brief Deinitialize the generic event system
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t drv_loop_deinit(void);

/**
 * @brief Register an event handler for any event base and ID
 *
 * @param event_base Event base to listen for
 * @param event_id Event ID to listen for (or ESP_EVENT_ANY_ID)
 * @param event_handler Handler function to call
 * @param event_handler_arg Argument to pass to handler
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t drv_loop_register_handler(esp_event_base_t event_base,
                                   int32_t event_id,
                                   esp_event_handler_t event_handler,
                                   void* event_handler_arg);

/**
 * @brief Unregister an event handler
 *
 * @param event_base Event base
 * @param event_id Event ID
 * @param event_handler Handler function to unregister
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t drv_loop_unregister_handler(esp_event_base_t event_base,
                                     int32_t event_id,
                                     esp_event_handler_t event_handler);

/**
 * @brief Post an event with arbitrary data
 *
 * @param event_base Event base
 * @param event_id Event ID
 * @param event_data Pointer to event data
 * @param event_data_size Size of event data
 * @param ticks_to_wait Ticks to wait for posting
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t drv_loop_post_event(esp_event_base_t event_base,
                             int32_t event_id,
                             const void* event_data,
                             size_t event_data_size,
                             TickType_t ticks_to_wait);

/**
 * @brief Get the event loop handle for advanced use cases
 *
 * @return esp_event_loop_handle_t The event loop handle, or NULL if not initialized
 */
esp_event_loop_handle_t drv_loop_get_handle(void);

#ifdef __cplusplus
}
#endif

#endif // DRV_LOOP_H
