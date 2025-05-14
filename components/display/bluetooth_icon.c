/*
 *  This program is a keyboard firmware for ESP family boards inspired by
 *  MK32 and qmk_firmware.
 *
 *  Copyright (C) 2024 github.com/paul356
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifdef __has_include
    #if __has_include("lvgl.h")
        #ifndef LV_LVGL_H_INCLUDE_SIMPLE
            #define LV_LVGL_H_INCLUDE_SIMPLE
        #endif
    #endif
#endif

#if defined(LV_LVGL_H_INCLUDE_SIMPLE)
    #include "lvgl.h"
#else
    #include "lvgl/lvgl.h"
#endif


#ifndef LV_ATTRIBUTE_MEM_ALIGN
#define LV_ATTRIBUTE_MEM_ALIGN
#endif

#ifndef LV_ATTRIBUTE_IMG_BLUETOOTH
#define LV_ATTRIBUTE_IMG_BLUETOOTH
#endif

const LV_ATTRIBUTE_MEM_ALIGN LV_ATTRIBUTE_LARGE_CONST LV_ATTRIBUTE_IMG_BLUETOOTH uint8_t bluetooth_icon_map[] = {
  0xff, 0xff, 0xff, 0xff, 	/*Color of index 0*/
  0x00, 0x00, 0x00, 0xff, 	/*Color of index 1*/

  0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 
  0x00, 0x60, 0x00, 
  0x00, 0x70, 0x00, 
  0x00, 0x70, 0x00, 
  0x00, 0x78, 0x00, 
  0x00, 0x7c, 0x00, 
  0x00, 0x6e, 0x00, 
  0x06, 0x77, 0x00, 
  0x0e, 0x77, 0x00, 
  0x07, 0x67, 0x00, 
  0x03, 0xfe, 0x00, 
  0x01, 0xfc, 0x00, 
  0x00, 0xf8, 0x00, 
  0x00, 0xf0, 0x00, 
  0x01, 0xf8, 0x00, 
  0x03, 0xfc, 0x00, 
  0x07, 0x6e, 0x00, 
  0x0f, 0x77, 0x00, 
  0x06, 0x77, 0x00, 
  0x00, 0x6e, 0x00, 
  0x00, 0x7c, 0x00, 
  0x00, 0x7c, 0x00, 
  0x00, 0x78, 0x00, 
  0x00, 0x70, 0x00, 
  0x00, 0x60, 0x00, 
  0x00, 0x40, 0x00, 
  0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 
};

const lv_img_dsc_t bluetooth_icon = {
  .header.cf = LV_IMG_CF_INDEXED_1BIT,
  .header.w = 21,
  .header.h = 32,
  .data_size = 104,
  .data = bluetooth_icon_map,
};
