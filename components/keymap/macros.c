#include "esp_log.h"
#include "nvs_funcs.h"
#include "macros.h"

#define TAG "MACROS"
#define MACRO_NAMESPACE "MACROS"

esp_err_t set_macro_str(uint16_t keycode, char* buf)
{
    int blob_len = strlen(buf) + 1;

    if (blob_len > MACRO_STR_MAX_LEN) {
        ESP_LOGE(TAG, "macro string is too long, len=%d", blob_len);
        return ESP_FAIL;
    }

    char scratch[8];
    int len = snprintf(scratch, sizeof(scratch), MACRO_NAME_PREFIX "%d", keycode);
    if (len >= sizeof(scratch) && len < 0) {
        ESP_LOGE(TAG, "keycode is too large, keycode=%u ", keycode);
        return ESP_FAIL;
    }

    return nvs_write_blob(MACRO_NAMESPACE, scratch, buf, blob_len);
}

esp_err_t get_macro_str(uint16_t keycode, char* buf, int buf_len)
{
    char scratch[8];
    int len = snprintf(scratch, sizeof(scratch), MACRO_NAME_PREFIX "%d", keycode);
    if (len >= sizeof(scratch) && len < 0) {
        ESP_LOGE(TAG, "keycode is too large, keycode=%u ", keycode);
        return ESP_FAIL;
    }

    size_t buf_size = buf_len;
    return nvs_read_blob(MACRO_NAMESPACE, scratch, buf, &buf_size);
}
