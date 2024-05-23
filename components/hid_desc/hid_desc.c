#include "tinyusb.h"

#define EPNUM_HID 0x4

enum {
    ITF_NUM_HID,
    ITF_NUM_TOTAL
};

enum {
    STRID_LANGID = 0,
    STRID_MANUFACTURER,
    STRID_PRODUCT,
    STRID_SERIAL,
    STRID_HID
};

enum {
    TUSB_DESC_TOTAL_LEN = TUD_CONFIG_DESC_LEN + TUD_HID_DESC_LEN
};

enum
{
  REPORT_ID_KEYBOARD = 1,
  REPORT_ID_MOUSE,
};

uint8_t const desc_hid_report[] = {
    TUD_HID_REPORT_DESC_KEYBOARD(HID_REPORT_ID(REPORT_ID_KEYBOARD) ),
    TUD_HID_REPORT_DESC_MOUSE(HID_REPORT_ID(REPORT_ID_MOUSE) )
};

static uint8_t const descriptor_hid_default[] = {
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, TUSB_DESC_TOTAL_LEN, TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),

    TUD_HID_DESCRIPTOR(ITF_NUM_HID, STRID_HID, 0, sizeof(desc_hid_report), 0x80 | EPNUM_HID, 16, 10)
};

static char * const descriptor_str_default[] = {
    // array of pointer to string descriptors
    (char[]){0x09, 0x04},                // 0: is supported language is English (0x0409)
    CONFIG_TINYUSB_DESC_MANUFACTURER_STRING, // 1: Manufacturer
    CONFIG_TINYUSB_DESC_PRODUCT_STRING,      // 2: Product
    CONFIG_TINYUSB_DESC_SERIAL_STRING,       // 3: Serials, should use chip ID
    "esp32 keyboard"
};

void enable_usb_hid(void)
{
    tinyusb_config_t tusb_cfg = {
        .descriptor = descriptor_hid_default,
        .string_descriptor = descriptor_str_default,
        .external_phy = false
    };

    ESP_ERROR_CHECK(tinyusb_driver_install(&tusb_cfg));
}

uint8_t const *tud_hid_descriptor_report_cb(uint8_t itf)
{
    return desc_hid_report;
}

uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t* buffer, uint16_t reqlen)
{
  // TODO not Implemented
  (void) instance;
  (void) report_id;
  (void) report_type;
  (void) buffer;
  (void) reqlen;

  return 0;
}

// Invoked when received SET_REPORT control request or
// received data on OUT endpoint ( Report ID = 0, Type = 0 )
void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t const* buffer, uint16_t bufsize)
{
}
