# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)
set(PICO_SDK_PATH "/home/nandre/src/pico-sdk")

set(PICO_BOARD pico CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

if (PICO_SDK_VERSION_STRING VERSION_LESS "1.4.0")
  message(FATAL_ERROR "Raspberry Pi Pico SDK version 1.3.0 (or later) required. Your version is ${PICO_SDK_VERSION_STRING}")
endif()

project(dnvt-switch C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add executable. Default name is the project name, version 0.1

add_executable(dnvt-switch dnvt-switch.c ssd1306.c process_phones.c display.c load_calculator.c dev_lowlevel.c)
pico_generate_pio_header(dnvt-switch ${CMAKE_CURRENT_LIST_DIR}/differential_manchester.pio)

pico_set_program_name(dnvt-switch "dnvt-switch")
pico_set_program_version(dnvt-switch "0.1")

pico_enable_stdio_uart(dnvt-switch 1)
pico_enable_stdio_usb(dnvt-switch 0)
target_compile_definitions(dnvt-switch PRIVATE
	PICO_DEFAULT_UART_TX_PIN=12
        PICO_DEFAULT_UART_BAUD_RATE=115200
        PICO_DEFAULT_UART=0
	)

# Add the standard library to the build
target_link_libraries(dnvt-switch
        pico_stdlib)

add_compile_options(-Wall
        -Wno-format          # int != int32_t as far as the compiler is concerned because gcc has int32_t as long int
        -Wno-unused-function # we have some for the docs that aren't called
        -Wno-maybe-uninitialized
        )

# Add the standard include files to the build
target_include_directories(dnvt-switch PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}
  ${CMAKE_CURRENT_LIST_DIR}/.. # for our common lwipopts or any other standard includes, if required
)

# Add any user requested libraries
target_link_libraries(dnvt-switch
        hardware_i2c
        hardware_pio
        pico_multicore
        hardware_resets
        hardware_irq
        )

pico_add_extra_outputs(dnvt-switch)

