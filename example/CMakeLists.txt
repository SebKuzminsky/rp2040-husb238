cmake_minimum_required(VERSION 3.25)

# Pull in SDK (must be before project)
include("$ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake")
include("$ENV{PICO_EXAMPLES_PATH}/pico_extras_import_optional.cmake")

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

if (PICO_SDK_VERSION_STRING VERSION_LESS "1.5.1")
    message(FATAL_ERROR "Raspberry Pi Pico SDK version 1.5.1 (or later) required. Your version is ${PICO_SDK_VERSION_STRING}")
endif()

add_compile_options(
    -Wall
)

set(PROJECT_NAME "husb238-example")
project(${PROJECT_NAME} C CXX ASM)

pico_sdk_init()

add_subdirectory(../driver build.rp2040_husb238)


add_executable(
    cycle-pdos
    cycle-pdos.cpp
)

target_link_libraries(
    cycle-pdos
    pico_stdlib
    hardware_i2c
    rp2040_husb238
)

pico_enable_stdio_usb(cycle-pdos TRUE)
pico_enable_stdio_uart(cycle-pdos FALSE)

pico_add_extra_outputs(cycle-pdos)


add_executable(
    i2c-stress-test
    i2c-stress-test.cpp
)

target_link_libraries(
    i2c-stress-test
    pico_stdlib
    hardware_i2c
    rp2040_husb238
)

pico_enable_stdio_usb(i2c-stress-test TRUE)
pico_enable_stdio_uart(i2c-stress-test FALSE)

pico_add_extra_outputs(i2c-stress-test)
