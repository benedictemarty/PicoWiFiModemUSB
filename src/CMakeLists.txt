# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(PICO_SDK_PATH ${CMAKE_CURRENT_LIST_DIR}/pico-sdk)
set(PICO_EXTRAS_PATH ${CMAKE_CURRENT_LIST_DIR}/pico-extras)
set(PICO_TINYUSB_PATH ${CMAKE_CURRENT_LIST_DIR}/tinyusb)

include(${PICO_SDK_PATH}/pico_sdk_init.cmake)
add_subdirectory(${PICO_EXTRAS_PATH})

#add_subdirectory(src)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_BUILD_TYPE Debug)
set(PICO_BOARD pico_w)
set(PICO_PLATFORM rp2040)
set(PICO_COMPILER pico_arm_gcc)
set(PROJECT_NAME wifi_modem)
set(CYW43_HOST_NAME "PicoW2")

# Pull in Raspberry Pi Pico SDK (must be before project) 
#include(${PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(${PROJECT_NAME})

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Wokwi test variant: the simulator provides no USB host, so the modem console
# is routed to UART0 (ser_uart.c) instead of USB CDC (ser_cdc.c), and the boot
# skips the USB-ready wait. Enable with -DWOKWI=1. Off => normal USB-CDC build.
option(WOKWI "Wokwi test variant: UART console instead of USB CDC host" OFF)
if(WOKWI)
      set(SER_BACKEND ser_uart.c)
else()
      set(SER_BACKEND ser_cdc.c)
endif()

# Built-in trust store (certs/roots.pem, Mozilla) → DER + index in flash.
find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_custom_command(
      OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/roots_gen.c
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_LIST_DIR}/../tools/roots2c.py
              ${CMAKE_CURRENT_LIST_DIR}/../certs/roots.pem ${CMAKE_CURRENT_BINARY_DIR}/roots_gen.c
      DEPENDS ${CMAKE_CURRENT_LIST_DIR}/../certs/roots.pem ${CMAKE_CURRENT_LIST_DIR}/../tools/roots2c.py
      COMMENT "Generating roots_gen.c from certs/roots.pem"
      )

add_executable(${PROJECT_NAME}
      ${PROJECT_NAME}.cpp
      ${SER_BACKEND}
      usb_cdc.c
      lfs.c
      usb_descriptors.c
      roots_store.c
      roots_ca_cb.c
      ${CMAKE_CURRENT_BINARY_DIR}/roots_gen.c
      littlefs/lfs.c
      littlefs/lfs_util.c
      )

if(WOKWI)
      target_compile_definitions(${PROJECT_NAME} PRIVATE WOKWI_BUILD=1)
endif()
# Version: the VERSION file at the repository root is the single source.
file(STRINGS ${CMAKE_CURRENT_LIST_DIR}/../VERSION FW_VERSION LIMIT_COUNT 1)
# Build id, build date and clock floor (BUILD_EPOCH, see time_support.h) come
# from git, recomputed on every build — never from the build clock, so the
# same commit gives the same UF2 (reproducible build).
add_custom_target(fw_build_id
      COMMAND ${CMAKE_COMMAND} -DSRC=${CMAKE_CURRENT_LIST_DIR}/.. -DOUT=${CMAKE_CURRENT_BINARY_DIR}/build_id.h
              -P ${CMAKE_CURRENT_LIST_DIR}/../cmake/build_id.cmake
      BYPRODUCTS ${CMAKE_CURRENT_BINARY_DIR}/build_id.h
      COMMENT "Build id (git describe)"
      )
add_dependencies(${PROJECT_NAME} fw_build_id)
target_compile_definitions(${PROJECT_NAME} PRIVATE
      CYW43_HOST_NAME=\"${CYW43_HOST_NAME}\"
      LFS_NO_MALLOC=1
      LFS_NAME_MAX=64
      PICO_FLASH_SIZE_BYTES=2097152
      FW_VERSION=\"${FW_VERSION}\"
      MBEDTLS_CONFIG_FILE=\"mbedtls_config.h\"
      #PICO_STDIO_ENABLE_CRLF_SUPPORT
      )
target_include_directories(${PROJECT_NAME} PRIVATE
      ${CMAKE_CURRENT_LIST_DIR}/tinyusb/src
      ${CMAKE_CURRENT_LIST_DIR}
      ${CMAKE_CURRENT_BINARY_DIR}
      )
target_link_libraries(${PROJECT_NAME}
      pico_cyw43_arch_lwip_threadsafe_background
      pico_lwip_mbedtls
      pico_lwip_sntp
      pico_mbedtls
      pico_stdlib
      hardware_i2c
      tinyusb_device
      tinyusb_board
      )

pico_add_extra_outputs(${PROJECT_NAME})

# enable usb output, disable uart output
pico_enable_stdio_usb(${PROJECT_NAME} 0)
pico_enable_stdio_uart(${PROJECT_NAME} 1)
