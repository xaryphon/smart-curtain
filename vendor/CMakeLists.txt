include(get_cpm.cmake)

CPMAddPackage(
    NAME freertos_kernel
    URL  https://github.com/FreeRTOS/FreeRTOS-Kernel/releases/download/V10.6.2/FreeRTOS-KernelV10.6.2.zip
    URL_HASH SHA256=a3b1de0048ea57c0864fcf67552d6246ecc388a35332451ab8d865f768dea82f
    DOWNLOAD_ONLY YES
)

set(FREERTOS_KERNEL_PATH ${freertos_kernel_SOURCE_DIR})
include(${FREERTOS_KERNEL_PATH}/portable/ThirdParty/GCC/RP2040/FreeRTOS_Kernel_import.cmake)

