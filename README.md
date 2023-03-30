# A Keyboard Based On ESP32/ESP32-Sx Chips

### Introduction
ESP32 series chips are a popular IoT chips manufactured by [Espressif Inc.](https://www.espressif.com). These chips feature many easy to use interfaces like WiFi, Bluetooth, USB, UART. It makes them suitable candidates for keybaords and mouses. First I would need to thanks to two projects have inspire this project. One is [MK32](https://github.com/Galzai/MK32). The other is [qmk_firmware](https://github.com/qmk/qmk_firmware). I start by forking MK32 first, and then port qmk components /tmk_core/ and /quantum/ to this project and rewrite many components.

### Features
Until 2023/03/30 I have finished or largely finished these functionalities.
- Keyboard Logic (tmk_core and quantum)
- BLE Connection (not every stable)
- WiFi Hotspot
- A Web Server
  + Show keyboard status
  + Dynamicly change keymap
  + OTA interface
- I2C Display Support (LED controller 1306)

### Planned Features
- Beautify Web GUI (employ numl.design)
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
In ~build~ directory you will find a /esp32_keyboard.bin/. Flash this firmware to your esp32s3.

### Code Structure
- main
  + mk32_main.c (main function, still has mk32 in the name)
  + config.h (qmk config file)
  + keyboard_config.h (to be retired)
- components
  + ble (BT server implementation)
  + control (switches for functionalities)
  + nvsfuncs (utility functions for NVS storage)
  + quantum (QMK component)
  + tmk_core (QMK component)
  + display (display management)
  + keymap (QMK format keymap)
  + port_mgmt (Pin initialization and management)
  + web (Web server)
  + wifi (WiFi hot spot)
- dist (web UI)
  + index.html
  + app.js

### License
Sicne QMK firmware uses GPL-2.0. As required by this license I should also use GPL-2.0.