#pragma once

#include "esp_err.h"

esp_err_t init_display(void);

void update_display(uint16_t last_key);
