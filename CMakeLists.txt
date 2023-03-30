# For more information about build system see
# https://docs.espressif.com/projects/esp-idf/en/latest/api-guides/build-system.html
# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

set(EXTRA_COMPONENT_DIRS $ENV{IDF_PATH}/examples/common_components/protocol_examples_common)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(esp32_keyboard)
target_add_binary_data(esp32_keyboard.elf "dist/app.js" binary)
target_add_binary_data(esp32_keyboard.elf "dist/index.html" binary)
target_add_binary_data(esp32_keyboard.elf "dist/style.css" binary)

idf_build_set_property(COMPILE_OPTIONS -D__ESP32S3__ APPEND)
#idf_build_set_property(COMPILE_OPTIONS -DDEBUG_ACTION APPEND)
