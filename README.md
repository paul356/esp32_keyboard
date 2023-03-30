# A Keyboard Based On ESP32/ESP32-Sx Chips

### Introduction
ESP32 series chips are a popular IoT chips manufactured by [Espressif Inc.](https://www.espressif.com). These chips feature many easy to use interfaces like WiFi, Bluetooth, USB, UART. It makes them suitable candidates for keybaords and mouses. First I would need to thanks to two projects have inspire this project. One is [MK32](https://github.com/Galzai/MK32). The other is [qmk_firmware](https://github.com/qmk/qmk_firmware). I start by forking MK32 first, and then port qmk components /tmk_core/ and /quantum/ to this project and rewrite components.

I build a [Prenoic layout](https://olkb.com/collections/preonic) keyboard to test this project.

![img](https://paul356.github.io/images/esp_keyboard_example.jpg)

The web looks like this. Just a simple web page to test the functionalities.

![img](https://paul356.github.io/images/esp_keyboard_web.jpg)

This project can also be deployed on a esp32-s3 dev-board like this.

![img](https://paul356.github.io/images/esp32_s3_board.jpg)

Merge requests are welcome. And if you have advice or feature request, please open a ticket.

### Features
Until now (2023/03/30) I have finished or barely finished these functionalities.
- Keyboard Logic (tmk_core and quantum)
- BLE Connection (not every stable)
- WiFi Hotspot
- A Web Server
  + Show Keyboard Status
  + Change Keymap Dynamicly
  + OTA Update
- I2C Display Support (LED controller 1306)

### Planned Features
- Beautify Web GUI (Employ [numl.design](https://numl.design/))
- Macro Definition Trough Web
- Key Recording (Controversial Feature)
- Power Resume (Can't wake up from USB suspend now)

### Build Process
It requires [a modified esp-idf version 4.4.4](https://github.com/paul356/esp-idf) to build this project. The changes are adding tinyusb HID support, esp_lvgl_port and lvgl 8.3. Please note Epressif has removed tinyusb from esp-idf 5.0. So esp-idf 5.0 can't be used. After you have esp-idf [installed and configured](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html#manual-installation), run following commands.
```
git clone https://github.com/paul356/esp32_keyboard.git
idf.py set-target esp32s3
idf.py build
idf.py flash
```
In /build/ directory you will find a /esp32_keyboard.bin/. Flash this firmware to your esp32s3.
```
idf.py flash
```
If you flash through UART, first put esp32s3 into download mode, then use this command.
```
esptool.py -p $1 -b 115200 --chip esp32s3  write_flash --flash_mode dio --flash_size detect --flash_freq 40m 0x0 build/bootloader/bootloader.bin 0x8000 build/partition_table/partition-table.bin 0xd000 build/ota_data_initial.bin 0x10000 build/MK32.bin 0x190000 build/esp32_keyboard.bin
```

### Code Structure
```
- main
  + mk32_main.c
  + config.h
  + keyboard_config.h
- components
  + ble
  + control
  + nvsfuncs
  + quantum
  + tmk_core
  + display
  + keymap
  + port_mgmt
  + web
  + wifi
- dist
  + index.html
  + app.js
```
/mk32_main.c/ contains the esp-idf main function. It is the place all components are put together. The code related to different functionalities are in /components/ folder.

### License
Since QMK firmware uses GPL-2.0 this project also uses GPL-2.0 as the open source license.