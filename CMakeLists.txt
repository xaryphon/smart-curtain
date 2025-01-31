cmake_minimum_required(VERSION 3.25)

set(PICO_BOARD pico_w)

include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(SmartCurtains C CXX ASM)

pico_sdk_init()

add_subdirectory(vendor)

add_executable(${PROJECT_NAME})

set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 17)

add_subdirectory(src)

set_target_properties(${PROJECT_NAME} PROPERTIES
    COMPILE_FLAGS "-Wall -Werror"
)

target_include_directories(${PROJECT_NAME} PRIVATE
    src
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    WIFI_SSID=\"$ENV{WIFI_SSID}\"
    WIFI_PASSWORD=\"$ENV{WIFI_PASSWORD}\"
    NO_SYS=0            # don't want NO_SYS (generally this would be in your lwipopts.h)
    PICO_CYW43_ARCH_DEFAULT_COUNTRY_CODE=CYW43_COUNTRY_FINLAND
)

target_link_libraries(${PROJECT_NAME}
    fmt::fmt
    FreeRTOS-Kernel-Heap4
    hardware_i2c
    hardware_adc
    hardware_spi
    pico_cyw43_arch_lwip_sys_freertos
    pico_lwip_mbedtls
    pico_mbedtls
    pico_stdlib
)

# Ignore warnings from lwip code
set_source_files_properties(
    ${PICO_LWIP_PATH}/src/apps/altcp_tls/altcp_tls_mbedtls.c
    PROPERTIES
    COMPILE_OPTIONS "-Wno-unused-result"
)

pico_add_uf2_output(${PROJECT_NAME})

pico_enable_stdio_usb(${PROJECT_NAME} 1)
pico_enable_stdio_uart(${PROJECT_NAME} 1)
