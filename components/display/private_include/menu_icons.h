#ifndef MENU_ICONS_H
#define MENU_ICONS_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

// External declarations for all menu icons
// Note: The actual icon variables will be declared with underscores instead of hyphens
extern const lv_image_dsc_t keyboard_icon;
extern const lv_image_dsc_t bluetooth_icon;
extern const lv_image_dsc_t wifi_icon;
extern const lv_image_dsc_t led_icon;
extern const lv_image_dsc_t advanced_icon;
extern const lv_image_dsc_t info_icon;

#ifdef __cplusplus
}
#endif

#endif // MENU_ICONS_H
