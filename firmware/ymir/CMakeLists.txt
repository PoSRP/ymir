cmake_minimum_required(VERSION 3.22)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

project(ymir)
message("Build type: ${CMAKE_BUILD_TYPE}")

enable_language(C CXX ASM)

# Workaround: gcc incorrectly pulls in libob.a when C++ files are present.
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)

# Create the executable before including the CubeMX subdirectory.
# cmake/stm32cubemx/CMakeLists.txt appends sources and links libraries to
# ${CMAKE_PROJECT_NAME} by name, so the target must already exist.
add_executable(${CMAKE_PROJECT_NAME})

target_link_options(${CMAKE_PROJECT_NAME} PRIVATE
    "-T${CMAKE_SOURCE_DIR}/../linker/bootloader.ld"
)

# Pull in CubeMX-generated library targets (STM32_Drivers,
# USB_Device_Library, stm32cubemx) and apply generated sources and
# include paths to our executable.
add_subdirectory("${CMAKE_SOURCE_DIR}/../cubemx/cmake/stm32cubemx" "cubemx")

target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    src/main.cpp
    src/boot_manager.cpp
    src/boot_jump.cpp
    src/flash.cpp
    src/flash_hw.cpp
    src/crc32.cpp
    src/metadata.cpp
    src/sha256.cpp
    src/image.cpp
    src/usb_transport.cpp
    src/protocol.cpp
    src/dfu_led_hw.cpp
)

target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    src
    include
)

# Generate a flat .bin alongside the .elf for flashing
add_custom_command(TARGET ${CMAKE_PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary
        $<TARGET_FILE:${CMAKE_PROJECT_NAME}>
        $<TARGET_FILE_DIR:${CMAKE_PROJECT_NAME}>/${CMAKE_PROJECT_NAME}.bin
    COMMENT "Generating ${CMAKE_PROJECT_NAME}.bin"
)

add_custom_command(TARGET ${CMAKE_PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:${CMAKE_PROJECT_NAME}>
)
