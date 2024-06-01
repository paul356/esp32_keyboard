# A Keyboard Based On ESP32/ESP32-Sx Chips

### Introduction
ESP32 series chips are a popular IoT chips manufactured by [Espressif Inc.](https://www.espressif.com). These chips feature many easy to use interfaces like WiFi, Bluetooth, USB, UART. It makes them suitable candidates for keybaords and mouses. This project is inspired by two projects. One is [MK32](https://github.com/Galzai/MK32). The other is [qmk_firmware](https://github.com/qmk/qmk_firmware). I start by forking MK32 first, and then port qmk components `tmk_core` and `quantum` to this project and rewrite many components.

I build a [Prenoic](https://olkb.com/collections/preonic) keyboard to test this project.

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
  + Support Dynamic Macros. Maros can be changed from the web interface.
  ![img](https://paul356.github.io/images/esp_keyboard_macros.jpg)
  + OTA Update
  ![img](https://paul356.github.io/images/esp_keyboard_ota.jpg)
- I2C Display Support (LED controller 1306)
- Function Keys
  + Print Device Info
  + Print Welcome Introduction

### Planned Features
- Beautify Web GUI (use [numl.design](https://numl.design/))
- Key Recording
- Support Wifi Configuration

### Build Process
It builds with esp-idf 5.3. It used to build with a customized esp-idf 4.4 for version v0.0.1. After you have esp-idf [installed and configured](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html#manual-installation) installed, run following commands. Note `get_idf` is the bash alias for entering esp-idf environment.
```
git clone https://github.com/paul356/esp32_keyboard.git
get_idf
idf.py set-target esp32s3
idf.py build
idf.py flash
```
In `build` directory you will find the bin file `esp32_keyboard.bin`. Flash this firmware to your esp32s3 development board assuming you connect to your board with ESPLink.
```
idf.py flash
```
If you flash through an USB to UART adaptor, first put esp32s3 into download mode (hold BOOT then press reset), then use this command to flash the board.
```
esptool.py -p $1 -b 115200 --chip esp32s3  write_flash --flash_mode dio --flash_size detect --flash_freq 40m 0x0 build/bootloader/bootloader.bin 0x8000 build/partition_table/partition-table.bin 0xd000 build/ota_data_initial.bin 0x10000 build/esp32_keyboard.bin 0x1a0000 build/esp32_keyboard.bin
```

### Code Structure
```
- main
  + mk32_main.c
- components
  + ble
  + control
  + display
  + hid_desc
  + keymap
  + nvsfuncs
  + port_mgmt
  + quantum
  + tmk_core
  + web
  + wifi
- resources
  + index.html
  + app.js
```
`mk32_main.c` contains the esp-idf main function. It is the place all components are put together. The code related to different functionalities are in `components` folder. `resources` contains the static web page and the javascript.

### License
Since QMK firmware uses GPL-2.0 this project also uses GPL-2.0 as the open source license.
