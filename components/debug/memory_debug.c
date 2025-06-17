/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 *
 * Copyright 2018 Gal Zaidenstein.
 */

#include "memory_debug.h"

static const char* TAG = "MEMORY_DEBUG";

void log_heap_usage(const char* stage_name, uint32_t caps, const char* caps_name)
{
    size_t free_size = heap_caps_get_free_size(caps);
    size_t total_size = heap_caps_get_total_size(caps);
    size_t largest_free = heap_caps_get_largest_free_block(caps);
    size_t minimum_free = heap_caps_get_minimum_free_size(caps);

    ESP_LOGI(TAG, "    %s Memory:", caps_name);
    ESP_LOGI(TAG, "      Free: %u bytes (%.1f%%)", free_size,
             total_size > 0 ? (100.0 * free_size / total_size) : 0.0);
    ESP_LOGI(TAG, "      Total: %u bytes", total_size);
    ESP_LOGI(TAG, "      Largest free block: %u bytes", largest_free);
    ESP_LOGI(TAG, "      Minimum free (since boot): %u bytes", minimum_free);
}

void log_memory_usage(const char* stage_name)
{
    ESP_LOGI(TAG, "=== Memory Usage Report: %s ===", stage_name);

    // Internal RAM (fast, used for critical operations)
    log_heap_usage(stage_name, MALLOC_CAP_INTERNAL, "INTERNAL RAM");

    // SPIRAM/PSRAM (external, larger but slower)
    if (heap_caps_get_total_size(MALLOC_CAP_SPIRAM) > 0) {
        log_heap_usage(stage_name, MALLOC_CAP_SPIRAM, "SPIRAM/PSRAM");
    }

    // DMA capable memory (subset of internal, can be used by DMA)
    log_heap_usage(stage_name, MALLOC_CAP_DMA, "DMA CAPABLE");

    // 32-bit aligned memory
    log_heap_usage(stage_name, MALLOC_CAP_32BIT, "32-BIT ALIGNED");

    // All memory combined
    log_heap_usage(stage_name, MALLOC_CAP_DEFAULT, "TOTAL");

    ESP_LOGI(TAG, "=== End Memory Report: %s ===", stage_name);
}
