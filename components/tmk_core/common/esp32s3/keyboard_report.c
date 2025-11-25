#include "host.h"
#include "keyboard_report.h"
#include "wait.h"
#include "tinyusb.h"
#include "esp_log.h"
#include "hal_ble.h"
#include "ble_events.h"  // Include BLE-specific events
#include "keyboard_gui.h"
#include "hid_desc.h"
#include "keycode_config.h"
#include "report.h"

static void send_keyboard_to_queue(report_keyboard_t*);
static uint8_t keyboard_leds_status(void);
static host_driver_t keyboard_driver = {.send_keyboard = send_keyboard_to_queue, .keyboard_leds = keyboard_leds_status};

int register_keyboard_reporter(void)
{
    host_set_driver(&keyboard_driver);
    return 0;
}

static uint8_t keyboard_leds_status(void)
{
    return is_caps_on() ? 0x2 : 0;
}

static void send_keyboard_to_queue(report_keyboard_t *report)
{
    uint8_t report_state[MAX_REPORT_LEN] = {0};
    uint8_t report_len = 0;
    uint8_t report_data_offset = 0;

    int intf_num;
    int report_id;

#ifdef NKRO_ENABLE
    if (!is_boot_protocol() && keymap_config.nkro)
    {
        // Check if the report was modified, if so send it
        report_state[0] = report->nkro.mods;

        memcpy(&report_state[1], report->nkro.bits, KEYBOARD_REPORT_BITS);

        report_len = KEYBOARD_REPORT_BITS + 1;
        report_data_offset = 1;
        intf_num = ITF_NUM_COMPOSITE;
        report_id = REPORT_ID_NKRO;
    }
    else
#endif
    {
        report_state[0] = report->mods;
        report_state[1] = report->reserved;

        int index = 0;
        for (uint16_t i = 0; i < KEYBOARD_REPORT_KEYS; i++)
        {
            if (report->keys[i])
            {
                report_state[index + BOOT_REPORT_OFFSET] = report->keys[i];
                index++;
            }
        }

        report_len = BOOT_REPORT_LEN;
        report_data_offset = BOOT_REPORT_OFFSET;
        if (is_boot_protocol())
        {
            intf_num = ITF_NUM_BOOT_KB;
            report_id = 0; // In boot protocol, report ID is not used
        }
        else
        {
            intf_num = ITF_NUM_COMPOSITE;
            report_id = REPORT_ID_KEYBOARD;
        }
    }

    if (keyboard_gui_handle_key_input(report_state[0], &report_state[report_data_offset], report_len - report_data_offset, report_id == REPORT_ID_NKRO)) {
        // If the GUI handled the key input, we don't need to send it further
        return;
    }

    // hardcoded HID interface 0, keyboard id 1
    if (tud_ready()) {
        // endpoint may be busy
        while (!tud_hid_n_ready(0)) {
            wait_ms(1);
        }
        bool result;
        do {
            // tud_hid_n_report may be busy sometimes. Need to retry transmit again.
             result = tud_hid_n_report(intf_num, report_id, report_state, report_len);
             if (result == false) {
                wait_ms(1);
             } else {
                break;
             }
        } while (tud_hid_n_ready(0));
    }

    if (is_ble_ready()) {
        ble_post_keyboard_event(report_state, report_len, report_id == REPORT_ID_NKRO);
    }
}
