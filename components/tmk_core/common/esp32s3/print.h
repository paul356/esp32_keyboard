#pragma once

#define BIT4MASK 0xf

extern const char* bit4_rep[];

int tinyusb_cdc_print(const char*);

int tinyusb_cdc_vprintf(const char* fmt, ...);
