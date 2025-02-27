include(get_cpm.cmake)

CPMAddPackage(
    NAME freertos_kernel
    URL https://github.com/FreeRTOS/FreeRTOS-Kernel/releases/download/V10.6.2/FreeRTOS-KernelV10.6.2.zip
    URL_HASH SHA256=a3b1de0048ea57c0864fcf67552d6246ecc388a35332451ab8d865f768dea82f
    DOWNLOAD_ONLY YES
)

set(FREERTOS_KERNEL_PATH ${freertos_kernel_SOURCE_DIR})
include(${FREERTOS_KERNEL_PATH}/portable/ThirdParty/GCC/RP2040/FreeRTOS_Kernel_import.cmake)


CPMAddPackage(
    NAME fmt
    GIT_REPOSITORY https://github.com/fmtlib/fmt.git
    GIT_TAG 0c9fce2ffefecfdce794e1859584e25877b7b592 # 11.0.2
#    GIT_TAG 0e8aad961d66904cfda8d7cc894f6f6eee2d9f30 # 11.1.0 -- compiler fails due to not finding some 'lockfiles'
    OPTIONS
    "FMT_DOC OFF"
    "FMT_TEST OFF"
)


CPMAddPackage(
    NAME picohttpparser
    GIT_REPOSITORY https://github.com/h2o/picohttpparser.git
    GIT_TAG f8d0513f1a7a111f2597d643b073935a8afaf9e5 # master 2025-01-30T08:30:00Z
    DOWNLOAD_ONLY YES
)
add_library(picohttpparser STATIC ${picohttpparser_SOURCE_DIR}/picohttpparser.c)
target_include_directories(picohttpparser PUBLIC ${picohttpparser_SOURCE_DIR})


CPMAddPackage(
    NAME ArduinoJson
    GIT_REPOSITORY https://github.com/bblanchon/ArduinoJson.git
    GIT_TAG 8f7e793f37bff2ebcc806e92245f1540452ef9f3 # v7.3.0
    OPTIONS
    "BUILD_TESTING OFF"
)
