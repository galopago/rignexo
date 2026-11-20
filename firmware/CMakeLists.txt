cmake_minimum_required(VERSION 3.20)

if(NOT DEFINED TINYUSB_ROOT)
  if(DEFINED ENV{TINYUSB_ROOT})
    set(TINYUSB_ROOT "$ENV{TINYUSB_ROOT}")
  else()
    set(TINYUSB_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/external/tinyusb")
  endif()
endif()

if(NOT EXISTS "${TINYUSB_ROOT}/hw/bsp/family_support.cmake")
  message(FATAL_ERROR "TinyUSB not found at TINYUSB_ROOT=${TINYUSB_ROOT}. Run ./scripts/build.sh")
endif()

include(${TINYUSB_ROOT}/hw/bsp/family_support.cmake)

set(PROJECT_NAME rpi_pico_composite_sound_serial)
project(${PROJECT_NAME} C CXX ASM)

family_initialize_project(${PROJECT_NAME} ${CMAKE_CURRENT_LIST_DIR})

if(FAMILY STREQUAL "espressif")
  return()
endif()

add_executable(${PROJECT_NAME})

pico_generate_pio_header(${PROJECT_NAME} ${CMAKE_CURRENT_SOURCE_DIR}/src/mode_led.pio)

target_sources(${PROJECT_NAME} PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/src/audio_adc.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/audio_pwm.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/cat_kenwood.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/cdc_app.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/cdc_line_state.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/main.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/mode_led_pio.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/mode_store.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/ptt.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/uac2_app.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/usb_descriptors.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/vox.c
)

target_include_directories(${PROJECT_NAME} PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/src
)

family_configure_device_example(${PROJECT_NAME} noos)

target_link_libraries(${PROJECT_NAME} PRIVATE
  hardware_pwm
  hardware_adc
  hardware_pio
  hardware_flash
)
