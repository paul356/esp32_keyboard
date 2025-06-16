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

/**
 * @brief Print detailed memory usage information
 * @param stage_name Name of the initialization stage for logging
 */
void log_memory_usage(const char* stage_name);

/**
 * @brief Print memory usage for a specific heap capability
 * @param stage_name Name of the initialization stage for logging
 * @param caps Heap capability flags (e.g., MALLOC_CAP_INTERNAL, MALLOC_CAP_SPIRAM)
 * @param caps_name Human readable name for the capability
 */
void log_heap_usage(const char* stage_name, uint32_t caps, const char* caps_name);

#ifdef __cplusplus
}
#endif
