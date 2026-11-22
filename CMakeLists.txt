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
set(sdkVersion 2.3.1)
set(toolchainVersion 15_2_Rel1)
set(picotoolVersion 2.3.1)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico2_w CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(safe_project C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add executable. Default name is the project name, version 0.1

add_executable(safe_project
               safe_project.c 
               ssd1306.c)

pico_set_program_name(safe_project "safe_project")
pico_set_program_version(safe_project "0.1")

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(safe_project 0)
pico_enable_stdio_usb(safe_project 1)

# Add the standard library to the build
target_link_libraries(safe_project
        pico_stdlib
        pico_multicore
        hardware_flash
        hardware_sync
        hardware_spi
    )

# Add the standard include files to the build
target_include_directories(safe_project PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
)


#Add the LittleFS storage library source files
target_sources(safe_project PRIVATE
    cs390_lfs/pico_lfs_safe.c
    cs390_lfs/lfs.c
    cs390_lfs/lfs_util.c
    )   

# Add the LittleFS include folder
target_include_directories(safe_project PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/cs390_lfs
)    

pico_add_extra_outputs(safe_project)

