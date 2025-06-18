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

#pragma once

#include "esp_heap_caps.h"
#include "esp_log.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef DEBUG_MEMORY
/**
 * @brief Print detailed memory usage information
 * @param stage_name Name of the initialization stage for logging
 */
void log_memory_usage(const char* stage_name);
#else
#define log_memory_usage(stage_name) \
    do { \
        /* No-op in release builds */ \
    } while (0)
#endif // DEBUG_MEMORY

#ifdef __cplusplus
}
#endif
