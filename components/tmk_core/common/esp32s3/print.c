#include <string.h>
#if CONFIG_TINYUSB_CDC_ENABLED
#include "tusb_cdc_acm.h"

int tinyusb_cdc_print(const char* buf)
{
    int len = strlen(buf);
    
    tinyusb_cdcacm_itf_t cdc_acm_itf = TINYUSB_CDC_ACM_0;
    tinyusb_cdcacm_write_queue(cdc_acm_itf, (const uint8_t*)buf, len);
    tinyusb_cdcacm_write_flush(cdc_acm_itf, 0);

    return len;
}


static char buf[CONFIG_TINYUSB_CDC_RX_BUFSIZE / 2 + 1];
int tinyusb_cdc_vprintf(const char* fmt, ...)
{
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
#else
int tinyusb_cdc_print(const char* buf)
{
    return 0;
}

int tinyusb_cdc_vprintf(const char* fmt, ...)
{
    return 0;
}
#endif

const char* bit4_rep[16] = {
    [ 0] = "0000", [ 1] = "0001", [ 2] = "0010", [ 3] = "0011",
    [ 4] = "0100", [ 5] = "0101", [ 6] = "0110", [ 7] = "0111",
    [ 8] = "1000", [ 9] = "1001", [10] = "1010", [11] = "1011",
    [12] = "1100", [13] = "1101", [14] = "1110", [15] = "1111",
};
