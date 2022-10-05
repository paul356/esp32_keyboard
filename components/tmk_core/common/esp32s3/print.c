#include <string.h>
#include "tusb_cdc_acm.h"

int tinyusb_cdc_print(const char* buf)
{
    int len = strlen(buf);
    
    tinyusb_cdcacm_itf_t cdc_acm_itf = TINYUSB_CDC_ACM_0;
    tinyusb_cdcacm_write_queue(cdc_acm_itf, (const uint8_t*)buf, len);
    tinyusb_cdcacm_write_flush(cdc_acm_itf, 0);

    return len;
}

int tinyusb_cdc_vprintf(const char* fmt, ...)
{
    char buf[CONFIG_TINYUSB_CDC_RX_BUFSIZE / 2];
    va_list valist;

    va_start(valist, fmt);

    int chars = vsnprintf(buf, sizeof(buf), fmt, valist);
    if (chars + 1 > sizeof(buf)) {
        chars = sizeof(buf) - 1;
    }

    va_end(valist);

    if (chars < 0) {
        return chars;
    }

    tinyusb_cdcacm_itf_t cdc_acm_itf = TINYUSB_CDC_ACM_0;
    tinyusb_cdcacm_write_queue(cdc_acm_itf, (const uint8_t*)buf, chars);
    tinyusb_cdcacm_write_flush(cdc_acm_itf, 0);

    return chars;
}

