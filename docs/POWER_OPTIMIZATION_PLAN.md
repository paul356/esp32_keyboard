# Power Optimization Plan for ESP32 Keyboard Firmware

**Project:** MK32 ESP32 Keyboard
**Branch:** ninjia-keyboard-battery
**Date Created:** November 13, 2025
**Status:** Planning Phase

---

## Overview

This document outlines comprehensive power-saving strategies for the ESP32-based keyboard firmware. The goal is to maximize battery life while maintaining responsive keyboard operation and user experience.

### Current Power Status

**Existing Features:**
- ✅ Battery monitoring system (USB power detection, charging status, voltage ADC)
- ✅ Deep sleep support with `SLEEP_MINS` timeout
- ✅ Bluetooth modem sleep enabled
- ✅ Basic power management enabled
- ✅ TX power reduction configured

**Current Estimated Power Consumption:**
- Active with BLE: ~80-120mA
- Active with BLE + WiFi: ~150-250mA
- Deep Sleep: ~0.01-0.15mA

**Target After Optimization:**
- Active (battery optimized): ~20-40mA
- Idle: ~5-10mA
- Light Sleep: ~0.8-3mA
- Deep Sleep: ~0.01mA

**Expected Battery Life Improvement:** 3-5x longer runtime

---

## Optimization Strategies

### 1. Dynamic Frequency Scaling (DFS) ⚡ HIGH PRIORITY

**Status:** Not implemented (currently disabled)
**Expected Savings:** 30-50% during idle periods
**Difficulty:** Easy

**Implementation Location:** `main/mk32_main.c`

**Changes Required:**
```c
#include "esp_pm.h"

// In app_main(), after initialization
esp_pm_config_t pm_config = {
    .max_freq_mhz = 160,  // Full speed when needed
    .min_freq_mhz = 40,   // Scale down during idle (can try 10 MHz)
    .light_sleep_enable = true  // Enable automatic light sleep
};
esp_pm_configure(&pm_config);
```

**Configuration Changes:**
- Set `CONFIG_PM_DFS_INIT_AUTO=y` if using automatic mode
- Ensure `CONFIG_PM_ENABLE=y` (already set)

**Notes:**
- Start with `min_freq_mhz = 40` and test keyboard responsiveness
- Can be more aggressive (10 MHz) if no latency issues detected
- Light sleep will automatically engage during idle periods

---

### 2. Enable FreeRTOS Tickless Idle ⚡ HIGH PRIORITY

**Status:** Not implemented (currently disabled)
**Expected Savings:** 10-20% during low activity
**Difficulty:** Easy

**Implementation Location:** `sdkconfig.defaults`

**Changes Required:**
```ini
# Change from:
CONFIG_FREERTOS_USE_TICKLESS_IDLE=

# To:
CONFIG_FREERTOS_USE_TICKLESS_IDLE=y
```

**Notes:**
- Allows CPU to sleep between RTOS ticks
- No code changes needed, only configuration
- Works best with light sleep enabled
- Test to ensure timers and delays still work correctly

---

### 3. Display Power Management ⚡ HIGH PRIORITY

**Status:** Not implemented
**Expected Savings:** 10-30mA
**Difficulty:** Easy to Medium

**Implementation Location:** `components/display/keyboard_gui.c`

**Changes Required:**
1. Add display timeout functionality
2. Implement brightness control based on battery level
3. Add screen saver mode before complete shutdown

**New Functions to Add:**
```c
// In keyboard_gui.c
static uint32_t display_last_activity = 0;
static bool display_active = true;

void display_update_activity(void) {
    display_last_activity = esp_timer_get_time() / 1000; // Convert to ms
    if (!display_active) {
        display_enable();
        display_active = true;
    }
}

void display_check_timeout(void) {
    uint32_t current_time = esp_timer_get_time() / 1000;
    uint32_t timeout = 30000; // 30 seconds default

    // Adjust timeout based on power source
    if (!miscs_is_usb_powered()) {
        timeout = 15000; // 15 seconds on battery
    }

    if (display_active && (current_time - display_last_activity) > timeout) {
        display_disable();
        display_active = false;
    }
}

void display_set_brightness_by_battery(uint8_t battery_percentage) {
    if (battery_percentage < 10) {
        // Critical battery: minimum brightness
        display_set_brightness(10);
    } else if (battery_percentage < 30) {
        // Low battery: reduced brightness
        display_set_brightness(30);
    } else {
        // Normal operation
        display_set_brightness(100);
    }
}
```

**Integration Points:**
- Call `display_update_activity()` on any key press or encoder change
- Call `display_check_timeout()` in main keyboard loop or timer
- Call `display_set_brightness_by_battery()` periodically

---

### 4. LED Power Optimization ⚡ HIGH PRIORITY

**Status:** Partially implemented
**Expected Savings:** 20-80mA (depending on LED count and usage)
**Difficulty:** Easy

**Implementation Location:** `components/led_ctrl/`

**Changes Required:**
1. Reduce maximum brightness limits
2. Implement auto-disable on timeout
3. Battery-aware LED control
4. Prefer breathing patterns over solid colors

**New Functions to Add:**
```c
// In led_ctrl.c
static uint32_t led_last_activity = 0;
static uint8_t led_max_brightness = 100;

void led_ctrl_set_max_brightness_by_power_source(void) {
    if (miscs_is_usb_powered()) {
        led_max_brightness = 100; // Full brightness on USB
    } else {
        led_max_brightness = 30;  // Limited on battery
    }
}

void led_ctrl_set_brightness_by_battery(uint8_t battery_percentage) {
    if (battery_percentage < 10) {
        led_ctrl_disable(); // Turn off LEDs when critically low
    } else if (battery_percentage < 20) {
        led_ctrl_set_brightness(10); // Minimum brightness
    } else if (battery_percentage < 50) {
        led_ctrl_set_brightness(30); // Reduced brightness
    } else {
        led_ctrl_set_brightness(led_max_brightness); // Normal
    }
}

void led_ctrl_check_timeout(void) {
    uint32_t current_time = esp_timer_get_time() / 1000;
    uint32_t timeout = 60000; // 60 seconds on USB

    if (!miscs_is_usb_powered()) {
        timeout = 20000; // 20 seconds on battery
    }

    if ((current_time - led_last_activity) > timeout) {
        led_ctrl_disable();
    }
}

void led_ctrl_update_activity(void) {
    led_last_activity = esp_timer_get_time() / 1000;
    // Re-enable if was disabled
}
```

**Configuration Recommendations:**
- Default to breathing patterns (lower average power)
- Allow user to disable LEDs completely via keymap
- Store LED state in NVS for persistence

---

### 5. Conditional WiFi Management ⚡ HIGH PRIORITY

**Status:** Partially implemented (can be toggled)
**Expected Savings:** 80-120mA when disabled
**Difficulty:** Easy

**Implementation Location:** `components/wifi/wifi_intf.c` and `components/control/`

**Changes Required:**
1. Auto-disable WiFi after configuration/OTA update
2. Implement WiFi power saving modes when enabled
3. Add inactivity timeout for WiFi
4. Disable WiFi by default on battery power

**New Functions to Add:**
```c
// In wifi_intf.c
void wifi_set_power_save_mode(void) {
    if (is_wifi_enabled()) {
        // Use maximum modem sleep for power saving
        esp_wifi_set_ps(WIFI_PS_MAX_MODEM);
    }
}

void wifi_auto_disable_on_battery(void) {
    if (!miscs_is_usb_powered() && is_wifi_enabled()) {
        ESP_LOGI(TAG, "Disabling WiFi on battery power");
        update_wifi_switch(false);
    }
}

void wifi_check_inactivity_timeout(void) {
    static uint32_t wifi_last_activity = 0;
    uint32_t current_time = esp_timer_get_time() / 1000;
    uint32_t timeout = 300000; // 5 minutes

    if (is_wifi_enabled() &&
        (current_time - wifi_last_activity) > timeout) {
        ESP_LOGI(TAG, "WiFi inactivity timeout - disabling");
        update_wifi_switch(false);
    }
}
```

**Integration Points:**
- Call `wifi_auto_disable_on_battery()` on USB disconnect
- Call `wifi_check_inactivity_timeout()` periodically
- Update `wifi_last_activity` on web server requests
- Set power save mode immediately after WiFi enable

**User Experience:**
- Add visual indicator when WiFi is off to save power
- Allow easy re-enable via key combination
- Show warning before auto-disable

---

### 6. Optimize Bluetooth Power Consumption 🔋 MEDIUM PRIORITY

**Status:** Basic modem sleep enabled
**Expected Savings:** 5-15mA
**Difficulty:** Medium

**Implementation Location:** `components/ble/ble_gap.c`

**Changes Required:**
1. Increase connection interval during idle
2. Reduce advertising frequency after pairing
3. Disable BLE when USB is connected (optional)
4. Implement BLE power save mode

**New Functions to Add:**
```c
// In ble_gap.c
void ble_set_power_save_mode(bool power_save) {
    if (power_save) {
        // Increase connection intervals for power saving
        esp_ble_conn_update_params_t conn_params = {
            .min_int = 0x50,  // 100ms (0x50 * 1.25ms)
            .max_int = 0x80,  // 160ms (0x80 * 1.25ms)
            .latency = 4,     // Allow skipping 4 connection events
            .timeout = 400,   // 4 seconds supervision timeout
        };
        // Apply to active connections
        // esp_ble_gap_update_conn_params(&conn_params);
    } else {
        // Normal performance mode
        esp_ble_conn_update_params_t conn_params = {
            .min_int = 0x18,  // 30ms
            .max_int = 0x28,  // 50ms
            .latency = 0,
            .timeout = 400,
        };
        // Apply to active connections
    }
}

void ble_reduce_advertising_frequency(void) {
    // After initial pairing, reduce advertising frequency
    esp_ble_gap_config_adv_data_raw(adv_data, adv_data_len);
    // Increase advertising interval to save power
    // Default might be 100ms, increase to 1000ms after pairing
}

void ble_auto_manage_on_usb(void) {
    static bool last_usb_state = false;
    bool usb_powered = miscs_is_usb_powered();

    if (usb_powered && !last_usb_state) {
        // USB connected - optionally disable BLE
        // ESP_LOGI(TAG, "USB connected, consider disabling BLE");
        // update_ble_state(false, NULL);
    } else if (!usb_powered && last_usb_state) {
        // USB disconnected - ensure BLE is enabled
        if (!is_ble_enabled()) {
            update_ble_state(true, NULL);
        }
    }

    last_usb_state = usb_powered;
}
```

**Configuration Changes:**
- Consider `CONFIG_BT_CTRL_MODEM_SLEEP=y` (verify if available for ESP32-S3)
- Experiment with connection parameters for latency vs power trade-off

**Notes:**
- Test keyboard responsiveness with increased intervals
- May add slight latency (50-100ms) but saves significant power
- Consider user preference setting for performance vs battery

---

### 7. Smart Deep Sleep Implementation 🔋 MEDIUM PRIORITY

**Status:** Basic implementation exists
**Expected Savings:** 90-95% in deep sleep mode
**Difficulty:** Medium

**Implementation Location:** `main/mk32_main.c`

**Changes Required:**
1. USB-aware deep sleep (don't sleep when USB connected)
2. Different timeouts for USB vs battery
3. Proper peripheral shutdown before sleep
4. Battery level consideration

**Improved Implementation:**
```c
// In mk32_main.c
#define SLEEP_MINS_USB 30      // 30 minutes on USB
#define SLEEP_MINS_BATTERY 5   // 5 minutes on battery
#define SLEEP_MINS_LOW_BATTERY 2  // 2 minutes when battery < 20%

static void deep_sleep_improved(void *pvParameters) {
    uint64_t initial_time = esp_timer_get_time();
    uint64_t current_time_passed = 0;

    while (1) {
        current_time_passed = (esp_timer_get_time() - initial_time);

        // Reset timer on activity
        if (DEEP_SLEEP == false) {
            current_time_passed = 0;
            initial_time = esp_timer_get_time();
            DEEP_SLEEP = true;
        }

        // Determine timeout based on power source and battery level
        uint32_t sleep_timeout_mins;
        bool usb_powered = miscs_is_usb_powered();
        uint8_t battery_percentage = 0;

        if (usb_powered) {
            sleep_timeout_mins = SLEEP_MINS_USB;
        } else {
            miscs_get_battery_percentage(&battery_percentage);
            if (battery_percentage < 20) {
                sleep_timeout_mins = SLEEP_MINS_LOW_BATTERY;
            } else {
                sleep_timeout_mins = SLEEP_MINS_BATTERY;
            }
        }

        double timeout_seconds = SEC_TO_MIN * sleep_timeout_mins;

        if (((double)current_time_passed / USEC_TO_SEC) >= timeout_seconds) {
            if (DEEP_SLEEP == true) {
                ESP_LOGI(TAG, "Entering deep sleep (USB: %s, Battery: %d%%)",
                         usb_powered ? "Yes" : "No", battery_percentage);

                // Shutdown peripherals properly
                if (is_wifi_enabled()) {
                    update_wifi_switch(false);
                    vTaskDelay(100 / portTICK_PERIOD_MS);
                }

                led_ctrl_disable();
                display_disable();

#ifdef OLED_ENABLE
                vTaskDelay(20 / portTICK_PERIOD_MS);
                vTaskSuspend(xOledTask);
                deinit_oled();
#endif

                // Setup wake sources
                rtc_matrix_setup();
                esp_sleep_enable_touchpad_wakeup();
                esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON);

                // Enter deep sleep
                esp_deep_sleep_start();
            }
        }

        vTaskDelay(1000 / portTICK_PERIOD_MS); // Check every second
    }
}
```

**Notes:**
- Don't deep sleep when USB is connected and user might expect instant response
- More aggressive timeout on battery, especially when low
- Ensure clean shutdown of all peripherals before sleep
- Consider wake-up time when setting timeouts

---

### 8. Multi-Level Sleep Strategy 🔋 MEDIUM PRIORITY

**Status:** Not implemented
**Expected Savings:** Variable, optimizes for each scenario
**Difficulty:** Medium to High

**Implementation Location:** New file `components/power_mgmt/power_mgmt.c`

**Concept:**
Create a state machine with multiple power modes that automatically transition based on activity.

**Power Modes:**
```c
typedef enum {
    POWER_MODE_ACTIVE,      // Full performance, all features enabled
    POWER_MODE_IDLE,        // Reduced CPU freq, display timeout soon
    POWER_MODE_DROWSY,      // Display off, LEDs off, reduced BLE interval
    POWER_MODE_LIGHT_SLEEP, // CPU light sleep, quick wake on key
    POWER_MODE_DEEP_SLEEP   // Maximum savings, slow wake-up
} power_mode_t;
```

**Transition Timing (Battery Powered):**
- 0-30 seconds inactive: ACTIVE mode
- 30-120 seconds: IDLE mode
- 2-5 minutes: DROWSY mode
- 5-10 minutes: LIGHT_SLEEP mode
- 10+ minutes: DEEP_SLEEP mode

**Transition Timing (USB Powered):**
- 0-60 seconds: ACTIVE mode
- 60-300 seconds: IDLE mode
- 5-30 minutes: DROWSY mode
- 30+ minutes: DEEP_SLEEP mode (optional)

**Implementation Structure:**
```c
// In power_mgmt.c
typedef struct {
    power_mode_t current_mode;
    uint64_t last_activity_time;
    bool usb_powered;
    uint8_t battery_percentage;
} power_state_t;

static power_state_t power_state = {
    .current_mode = POWER_MODE_ACTIVE,
    .last_activity_time = 0,
    .usb_powered = true,
    .battery_percentage = 100,
};

void power_mgmt_init(void);
void power_mgmt_update_activity(void);
void power_mgmt_task(void *pvParameters);
power_mode_t power_mgmt_get_mode(void);
void power_mgmt_enter_mode(power_mode_t mode);
```

**Mode Behaviors:**
- **ACTIVE:** All features on, max CPU freq
- **IDLE:** CPU 80 MHz, display dimmed, full features
- **DROWSY:** CPU 40 MHz, display off, LEDs off, BLE power save
- **LIGHT_SLEEP:** Automatic light sleep between key scans
- **DEEP_SLEEP:** Complete shutdown, RTC wake only

**Integration:**
- Create dedicated FreeRTOS task for power management
- All user input calls `power_mgmt_update_activity()`
- Task monitors timeouts and transitions modes
- Each component responds to mode changes

---

### 9. Optimize Keyboard Scanning Rate 🔋 LOW PRIORITY

**Status:** Fixed 5ms rate
**Expected Savings:** 2-5mA
**Difficulty:** Medium

**Implementation Location:** `main/mk32_main.c`

**Changes Required:**
Make keyboard scan rate dynamic based on activity level.

**Implementation:**
```c
// In mk32_main.c
static esp_timer_handle_t keyboard_timer_handle = NULL;
static bool keyboard_active_mode = true;

void keyboard_set_scan_rate(bool active) {
    uint64_t period = active ? 5000 : 20000; // 5ms active, 20ms idle

    if (keyboard_timer_handle != NULL) {
        esp_timer_stop(keyboard_timer_handle);
        esp_timer_start_periodic(keyboard_timer_handle, period);
        keyboard_active_mode = active;

        ESP_LOGI(TAG, "Keyboard scan rate: %s", active ? "5ms" : "20ms");
    }
}

void keyboard_update_scan_rate_on_activity(void) {
    static uint64_t last_activity = 0;
    uint64_t current_time = esp_timer_get_time();

    // If there was a key press, ensure active mode for next 10 seconds
    if (!keyboard_active_mode) {
        keyboard_set_scan_rate(true);
    }

    last_activity = current_time;

    // After 10 seconds of no activity, reduce scan rate
    // This would be called from a monitoring task
}
```

**Alternative Approach:**
Use GPIO interrupts for wake-up from idle instead of continuous polling.

**Notes:**
- 20ms is still very responsive (50 Hz)
- Most users won't notice difference from 5ms to 10-20ms
- Could go even slower (50ms) during drowsy mode
- Matrix scanning is relatively low power anyway

---

### 10. USB vs Battery Mode Switching ⚡ HIGH PRIORITY

**Status:** Hardware detection exists, mode switching not implemented
**Expected Savings:** Contextual, enables all other optimizations
**Difficulty:** Easy to Medium

**Implementation Location:** New file `components/power_mgmt/power_mgmt.c` or `main/mk32_main.c`

**Changes Required:**
Central function that switches between USB and battery profiles.

**Implementation:**
```c
// In power_mgmt.c
void power_mode_update_by_source(void) {
    static bool last_usb_state = true;
    bool usb_powered = miscs_is_usb_powered();
    uint8_t battery_percentage = 0;
    miscs_get_battery_percentage(&battery_percentage);

    // Detect state change
    if (usb_powered != last_usb_state) {
        ESP_LOGI(TAG, "Power source changed: %s",
                 usb_powered ? "USB" : "Battery");
        last_usb_state = usb_powered;
    }

    if (usb_powered) {
        // ============= USB POWER MODE =============
        ESP_LOGI(TAG, "Applying USB power profile");

        // CPU: Full performance
        esp_pm_config_t pm_config = {
            .max_freq_mhz = 160,
            .min_freq_mhz = 160,  // No downclocking on USB
            .light_sleep_enable = false
        };
        esp_pm_configure(&pm_config);

        // LEDs: Full brightness available
        led_ctrl_set_max_brightness_by_power_source();

        // Display: Normal timeout (30s)
        display_set_timeout(30000);

        // WiFi: Can be enabled if needed
        // (User controlled, but don't auto-disable)

        // BLE: Normal connection parameters
        ble_set_power_save_mode(false);

        // Keyboard: Fast scan rate
        keyboard_set_scan_rate(true);

    } else {
        // ============= BATTERY POWER MODE =============
        ESP_LOGI(TAG, "Applying battery power profile (Battery: %d%%)",
                 battery_percentage);

        // CPU: Dynamic frequency scaling
        esp_pm_config_t pm_config = {
            .max_freq_mhz = 160,
            .min_freq_mhz = 40,   // Aggressive downclocking
            .light_sleep_enable = true
        };
        esp_pm_configure(&pm_config);

        // LEDs: Reduced brightness or off
        led_ctrl_set_max_brightness_by_power_source();
        led_ctrl_set_brightness_by_battery(battery_percentage);

        // Display: Shorter timeout (15s)
        display_set_timeout(15000);
        display_set_brightness_by_battery(battery_percentage);

        // WiFi: Auto-disable on battery
        if (is_wifi_enabled()) {
            ESP_LOGI(TAG, "Auto-disabling WiFi on battery");
            update_wifi_switch(false);
        }

        // BLE: Power save mode
        ble_set_power_save_mode(true);

        // Keyboard: Normal scan rate initially
        keyboard_set_scan_rate(true);

        // Critical battery handling
        if (battery_percentage < 10) {
            ESP_LOGW(TAG, "CRITICAL BATTERY - Extreme power saving");
            led_ctrl_disable();
            display_disable();
            // Consider showing low battery warning before shutdown
        }
    }
}
```

**Integration:**
- Call once during startup
- Call periodically (every 5-10 seconds) to detect USB connection changes
- Call after wake from sleep
- Trigger on battery level changes

---

### 11. Battery-Aware Power Profiles 🔋 MEDIUM PRIORITY

**Status:** Not implemented
**Expected Savings:** Graceful degradation at low battery
**Difficulty:** Easy

**Implementation Location:** `components/power_mgmt/power_mgmt.c`

**Changes Required:**
Implement tiered responses based on battery level.

**Battery Level Thresholds:**
```c
#define BATTERY_CRITICAL    10  // 10% - Minimal features only
#define BATTERY_LOW         20  // 20% - Reduced features
#define BATTERY_MEDIUM      50  // 50% - Moderate power saving
#define BATTERY_GOOD        100 // 100% - Normal operation

void apply_battery_profile(uint8_t battery_percentage) {
    if (miscs_is_usb_powered()) {
        // Don't apply battery profiles when on USB
        return;
    }

    if (battery_percentage <= BATTERY_CRITICAL) {
        // ============= CRITICAL BATTERY =============
        ESP_LOGW(TAG, "CRITICAL BATTERY: %d%%", battery_percentage);

        // Disable all non-essential features
        led_ctrl_disable();
        display_disable();

        if (is_wifi_enabled()) {
            update_wifi_switch(false);
        }

        // BLE only, ultra-low power mode
        ble_set_power_save_mode(true);

        // Minimum CPU frequency
        esp_pm_config_t pm_config = {
            .max_freq_mhz = 80,   // Reduced max
            .min_freq_mhz = 10,   // Absolute minimum
            .light_sleep_enable = true
        };
        esp_pm_configure(&pm_config);

        // Show critical battery warning to user
        // Consider forced shutdown or deep sleep

    } else if (battery_percentage <= BATTERY_LOW) {
        // ============= LOW BATTERY =============
        ESP_LOGW(TAG, "LOW BATTERY: %d%%", battery_percentage);

        // Reduced features
        led_ctrl_set_brightness(10);  // Minimum LED
        display_set_brightness(20);   // Dim display
        display_set_timeout(10000);   // Quick timeout (10s)

        if (is_wifi_enabled()) {
            update_wifi_switch(false);  // No WiFi
        }

        ble_set_power_save_mode(true);

        esp_pm_config_t pm_config = {
            .max_freq_mhz = 120,
            .min_freq_mhz = 20,
            .light_sleep_enable = true
        };
        esp_pm_configure(&pm_config);

    } else if (battery_percentage <= BATTERY_MEDIUM) {
        // ============= MEDIUM BATTERY =============
        ESP_LOGI(TAG, "MEDIUM BATTERY: %d%%", battery_percentage);

        // Moderate power saving
        led_ctrl_set_brightness(30);
        display_set_brightness(50);
        display_set_timeout(15000);   // 15s timeout

        ble_set_power_save_mode(true);

        esp_pm_config_t pm_config = {
            .max_freq_mhz = 160,
            .min_freq_mhz = 40,
            .light_sleep_enable = true
        };
        esp_pm_configure(&pm_config);

    } else {
        // ============= GOOD BATTERY =============
        ESP_LOGI(TAG, "GOOD BATTERY: %d%%", battery_percentage);

        // Normal battery operation (still optimized)
        led_ctrl_set_brightness(50);
        display_set_brightness(80);
        display_set_timeout(20000);   // 20s timeout

        ble_set_power_save_mode(true);

        esp_pm_config_t pm_config = {
            .max_freq_mhz = 160,
            .min_freq_mhz = 40,
            .light_sleep_enable = true
        };
        esp_pm_configure(&pm_config);
    }
}
```

**User Notification:**
- Show battery level on display
- Warn user at 20%, 10%, 5%
- Different LED patterns for battery levels
- Consider audio beep at critical level (if speaker available)

---

### 12. Peripheral Power Control 🔋 LOW PRIORITY

**Status:** Not implemented
**Expected Savings:** 2-5mA
**Difficulty:** Medium

**Implementation Location:** Various component files

**Changes Required:**
Selectively disable peripherals when not in use.

**Peripherals to Manage:**
```c
// In power_mgmt.c
void disable_unused_peripherals(void) {
    // ADC: Only enable when reading battery
    // (Already done in miscs.c - read and disable)

    // Encoder: Could reduce interrupt priority or disable in deep sleep
    // (Keep enabled - needed for wake-up)

    // GPIO: Configure for minimum leakage
    // Disable pull-ups/downs on unused pins
    for (int gpio = 0; gpio < GPIO_NUM_MAX; gpio++) {
        // Check if GPIO is used, if not, configure for low power
        // gpio_set_pull_mode(gpio, GPIO_FLOATING);
    }

    // UART: Disable if not debugging
    // uart_driver_delete(UART_NUM_0);

    // SPI/I2C: Disable if display is off
    // spi_bus_free(SPI_HOST);
}

void enable_required_peripherals(void) {
    // Re-enable peripherals needed for operation
    // Called on wake-up or mode change
}
```

**ADC Power Optimization:**
```c
// In miscs.c - modify battery reading function
esp_err_t miscs_get_battery_percentage(uint8_t *percentage) {
    // Enable ADC only for reading
    // (Already handled by oneshot API)

    // Read voltage
    esp_err_t ret = read_battery_voltage_internal(percentage);

    // ADC automatically disabled by oneshot API after read

    return ret;
}
```

**Notes:**
- Most peripherals already have good power management
- Focus on peripherals that stay powered unnecessarily
- Be careful not to break functionality
- Test thoroughly after disabling anything

---

### 13. Periodic Battery Monitoring 🔋 MEDIUM PRIORITY

**Status:** Hardware exists, monitoring strategy needed
**Expected Savings:** Enables other optimizations
**Difficulty:** Easy

**Implementation Location:** `main/mk32_main.c` or power management component

**Changes Required:**
Create a task to monitor battery and trigger profile changes.

**Implementation:**
```c
// In power_mgmt.c or mk32_main.c
static void battery_monitor_task(void *pvParameters) {
    uint8_t last_battery_percentage = 100;
    const uint32_t check_interval_ms = 60000; // Check every 60 seconds

    while (1) {
        uint8_t battery_percentage = 0;
        esp_err_t ret = miscs_get_battery_percentage(&battery_percentage);

        if (ret == ESP_OK) {
            // Check for significant change (5% threshold)
            if (abs(battery_percentage - last_battery_percentage) >= 5) {
                ESP_LOGI(TAG, "Battery level changed: %d%% -> %d%%",
                         last_battery_percentage, battery_percentage);

                // Update power profile
                apply_battery_profile(battery_percentage);

                // Update display if showing battery
                // update_battery_display(battery_percentage);

                last_battery_percentage = battery_percentage;
            }

            // Always log battery status
            ESP_LOGI(TAG, "Battery: %d%%, USB: %s, Charging: %s",
                     battery_percentage,
                     miscs_is_usb_powered() ? "Yes" : "No",
                     miscs_is_battery_charging() ? "Yes" : "No");

        } else {
            ESP_LOGE(TAG, "Failed to read battery: %s", esp_err_to_name(ret));
        }

        // Check for USB connection change
        power_mode_update_by_source();

        vTaskDelay(check_interval_ms / portTICK_PERIOD_MS);
    }
}

// In app_main()
void start_battery_monitor(void) {
    xTaskCreate(battery_monitor_task, "battery_monitor",
                2048, NULL, 5, NULL);
}
```

**Battery Display Integration:**
- Show battery percentage on display
- Battery icon with charge level
- Charging indicator when USB connected
- Low battery warning overlay

---

## Implementation Priority Matrix

### Phase 1: Quick Wins (Week 1-2)
**Effort: Low | Impact: High**

1. ✅ Enable FreeRTOS Tickless Idle (configuration only)
2. ✅ Enable Dynamic Frequency Scaling (small code addition)
3. ✅ USB vs Battery Mode Switching (central optimization)
4. ✅ Display timeout (straightforward timer)
5. ✅ LED brightness limits on battery

**Expected improvement:** 40-60% power reduction

---

### Phase 2: Core Optimizations (Week 3-4)
**Effort: Medium | Impact: High**

1. ✅ Conditional WiFi management
2. ✅ Battery-aware power profiles
3. ✅ Improved deep sleep logic
4. ✅ Periodic battery monitoring task
5. ✅ BLE power optimization

**Expected improvement:** Additional 20-30% reduction

---

### Phase 3: Advanced Features (Week 5-6)
**Effort: High | Impact: Medium**

1. ⚠️ Multi-level sleep strategy
2. ⚠️ Dynamic keyboard scan rate
3. ⚠️ Peripheral power gating
4. ⚠️ Advanced BLE connection management

**Expected improvement:** Additional 10-20% reduction (diminishing returns)

---

## Testing & Validation

### Power Measurement Setup
1. **Equipment needed:**
   - USB power monitor (e.g., USB-C power meter)
   - Multimeter for direct battery measurement
   - Oscilloscope for current profiling (optional)

2. **Test scenarios:**
   - Idle (no key presses)
   - Active typing
   - With/without LEDs
   - With/without display
   - BLE vs USB connection
   - WiFi on/off

3. **Baseline measurements:**
   - Record current consumption for each scenario before optimization
   - Create power profile graph

### Validation Tests
1. **Functionality:**
   - Ensure keyboard remains responsive
   - Verify all features work after power optimizations
   - Test wake from sleep modes
   - Verify USB connection detection

2. **Power consumption:**
   - Measure in each power mode
   - Compare against baseline
   - Calculate battery life improvement

3. **User experience:**
   - Check for latency issues
   - Verify display/LED behavior
   - Test transition smoothness between modes

---

## Configuration Reference

### Key sdkconfig Settings

```ini
# Power Management
CONFIG_PM_ENABLE=y
CONFIG_PM_DFS_INIT_AUTO=y              # NEW: Enable dynamic freq scaling
CONFIG_PM_USE_RTC_TIMER_REF=y          # NEW: Use RTC for timing in sleep

# FreeRTOS
CONFIG_FREERTOS_USE_TICKLESS_IDLE=y    # NEW: Enable tickless idle
CONFIG_FREERTOS_IDLE_TASK_STACKSIZE=1024
CONFIG_FREERTOS_HZ=1000

# Bluetooth
CONFIG_BTDM_CONTROLLER_MODEM_SLEEP=y
CONFIG_BT_CTRL_MODEM_SLEEP=y           # NEW: If available for ESP32-S3
CONFIG_BTDM_MODEM_SLEEP_MODE_ORIG=y

# Deep Sleep
CONFIG_ESP32_DEEP_SLEEP_WAKEUP_DELAY=2000

# WiFi
CONFIG_ESP32_WIFI_STATIC_RX_BUFFER_NUM=10
CONFIG_ESP32_WIFI_DYNAMIC_RX_BUFFER_NUM=32

# Power
CONFIG_REDUCE_PHY_TX_POWER=y
CONFIG_ESP32_PHY_MAX_TX_POWER=20
```

---

## Code Architecture

### Recommended New Component Structure

```
components/
  power_mgmt/
    CMakeLists.txt
    include/
      power_mgmt.h          # Public API
    private_include/
      power_modes.h         # Internal mode definitions
    power_mgmt.c            # Main power management
    power_modes.c           # Mode transition logic
    battery_monitor.c       # Battery monitoring task
```

### Key Data Structures

```c
// Central power state
typedef struct {
    power_mode_t current_mode;
    bool usb_powered;
    uint8_t battery_percentage;
    bool battery_charging;
    uint64_t last_activity_time;
    uint32_t time_in_current_mode;
} power_state_t;

// Power configuration
typedef struct {
    uint32_t display_timeout_ms;
    uint32_t led_timeout_ms;
    uint32_t wifi_timeout_ms;
    uint8_t led_max_brightness;
    uint8_t cpu_min_freq_mhz;
    bool ble_power_save;
} power_config_t;
```

---

## Known Issues & Considerations

### Potential Issues

1. **BLE Connection Stability**
   - Increasing connection intervals may cause issues with some hosts
   - Test thoroughly with target devices (Windows/Mac/Linux/Android/iOS)
   - May need user adjustable setting

2. **Display Flicker**
   - Rapid power mode changes could cause display issues
   - Implement debouncing/hysteresis for mode transitions

3. **USB Detection Reliability**
   - Verify GPIO6 reliably detects USB connection
   - May need debouncing if signal is noisy

4. **Wake Latency**
   - Deep sleep wake time is ~2 seconds (CONFIG_ESP32_DEEP_SLEEP_WAKEUP_DELAY)
   - Light sleep wake is much faster (~few ms)
   - Consider user expectations

### Design Decisions

1. **Aggressive vs Conservative**
   - Start conservative (less aggressive power saving)
   - Collect user feedback
   - Add "Power Save Mode" setting: Off/Balanced/Aggressive

2. **User Control**
   - Provide keymap functions to:
     - Toggle power save mode
     - Force display on/off
     - Disable LEDs manually
     - Show battery level on demand

3. **Persistence**
   - Save power preferences to NVS
   - Restore on boot
   - Per-profile settings (USB vs Battery)

---

## Monitoring & Debugging

### Debug Output

Add power management logging:
```c
#define PM_DEBUG_LOG 1

#if PM_DEBUG_LOG
#define PM_LOGD(fmt, ...) ESP_LOGD("POWER_MGMT", fmt, ##__VA_ARGS__)
#define PM_LOGI(fmt, ...) ESP_LOGI("POWER_MGMT", fmt, ##__VA_ARGS__)
#else
#define PM_LOGD(fmt, ...)
#define PM_LOGI(fmt, ...)
#endif
```

### Power Statistics

Implement runtime power statistics:
```c
typedef struct {
    uint64_t time_in_active_ms;
    uint64_t time_in_idle_ms;
    uint64_t time_in_light_sleep_ms;
    uint64_t time_in_deep_sleep_ms;
    uint32_t deep_sleep_count;
    uint32_t wake_count;
} power_stats_t;

// Expose via web interface or display
void power_mgmt_get_stats(power_stats_t *stats);
```

### Web Interface Integration

Add power management page to web server:
- Current power mode
- Battery level and status
- Power statistics
- Manual control overrides
- Power save settings

---

## Future Enhancements

### Advanced Features (Post-MVP)

1. **Machine Learning Power Management**
   - Learn user typing patterns
   - Predict idle periods
   - Optimize mode transitions

2. **Scheduled Power Modes**
   - Sleep during night hours
   - Different profiles for work/home

3. **Wireless Charging Support**
   - Detect wireless charging
   - Different behavior than wired USB

4. **Battery Health Monitoring**
   - Track charge cycles
   - Estimate battery degradation
   - Warn when battery needs replacement

5. **Power Consumption API**
   - Real-time power draw estimation
   - Log to file for analysis
   - Export to web interface

6. **Keyboard Backlighting Zones**
   - Per-key LED control for power saving
   - Only light active layer keys

---

## Resources & References

### ESP-IDF Documentation
- [Power Management](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/system/power_management.html)
- [Sleep Modes](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/system/sleep_modes.html)
- [Bluetooth Low Energy](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/bluetooth/index.html)

### Example Projects
- ESP-IDF examples: `examples/system/deep_sleep/`
- ESP-IDF examples: `examples/system/light_sleep/`
- ESP-IDF examples: `examples/bluetooth/bluedroid/ble/`

### Datasheets
- ESP32-S3 Datasheet (power consumption specifications)
- Battery specifications (capacity, discharge curves)

---

## Revision History

| Date | Version | Changes | Author |
|------|---------|---------|--------|
| 2025-11-13 | 1.0 | Initial power optimization plan | AI Assistant |

---

## Notes

- This is a living document - update as implementation progresses
- Record actual power measurements in this document
- Add lessons learned and gotchas
- Update priority based on real-world testing

**End of Power Optimization Plan**
