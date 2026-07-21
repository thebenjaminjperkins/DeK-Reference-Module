# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)

# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.3.0)
set(toolchainVersion 15_2_Rel1)
set(picotoolVersion 2.3.0)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico2_w CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(dek-reference-module C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

add_subdirectory(external/DeK-Protocol EXCLUDE_FROM_ALL)

# Add executable. Default name is the project name, version 0.1

add_executable(dek-reference-module
        uart_link_smoke.c
)

pico_set_program_name(dek-reference-module "dek-reference-module")
pico_set_program_version(dek-reference-module "0.1")

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(dek-reference-module 0)
pico_enable_stdio_usb(dek-reference-module 1)

# Add the standard library to the build
target_link_libraries(dek-reference-module
        pico_stdlib)

# Add the standard include files to the build
target_include_directories(dek-reference-module PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
)

# Add any user requested libraries
target_link_libraries(dek-reference-module 
        dek_protocol
        hardware_uart
        hardware_gpio
)

pico_add_extra_outputs(dek-reference-module)

