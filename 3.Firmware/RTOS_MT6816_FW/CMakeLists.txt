cmake_minimum_required(VERSION 3.22)

#
# This file is generated only once,
# and is not re-generated if converter is called multiple times.
#
# User is free to modify the file as much as necessary
#

# Setup compiler settings
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)


# Define the build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

# Set the project name
set(CMAKE_PROJECT_NAME RTOS_MT6816_FW)

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Core project settings
project(${CMAKE_PROJECT_NAME})
message("Build type: " ${CMAKE_BUILD_TYPE})

# Enable CMake support for ASM and C languages
enable_language(C ASM)

# Create an executable object type
add_executable(${CMAKE_PROJECT_NAME})

# Add STM32CubeMX generated sources
add_subdirectory(cmake/stm32cubemx)

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined library search paths
)

# Add sources to executable
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user sources here
        3rdParty/fibre/protocol.cpp

        Ctrl/Sensor/Encoder/mt6816_base.cpp
        Ctrl/Signal/LED/led_base.cpp
        Ctrl/Motor/motor.cpp
        Ctrl/Motor/math_utils.cpp

        Platform/CmdCtrlMotor/cmd_ctrl_motor.cpp
        Platform/Communication/ascii_processor.cpp
        Platform/Communication/communication.cpp
        Platform/Communication/interface_uart.cpp
        Platform/Communication/interface_usb.cpp
        Platform/Sensor/Encoder/mt6816_stm32.cpp
        Platform/Signal/LED/led_stm32.cpp
        Platform/Memory/random_flash_utils.cpp

        UserApp/main.cpp
        UserApp/protocols/ascii_protocol.cpp
        UserApp/protocols/cmd_protocol.cpp
)

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined include paths
        3rdParty/fibre/include

        Ctrl/Sensor/Encoder
        Ctrl/Signal/LED
        Ctrl/Motor

        Platform/CmdCtrlMotor
        Platform/Communication
        Platform/Sensor/Encoder
        Platform/Signal/LED
        Platform/Memory

        UserApp
        UserApp/protocols

)

# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined symbols
)

# Remove wrong libob.a library dependency when using cpp files
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx

    # Add user defined libraries
)
