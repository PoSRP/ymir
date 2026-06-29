cmake_minimum_required(VERSION 3.22)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

project(test_app_baremetal)
message("Build type: ${CMAKE_BUILD_TYPE}")

enable_language(C CXX ASM)

list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)

if(NOT DEFINED SLOT_LINKER_SCRIPT)
    message(FATAL_ERROR "SLOT_LINKER_SCRIPT must be set via a CMake preset")
endif()

add_executable(${CMAKE_PROJECT_NAME} src/main.cpp)

target_link_options(${CMAKE_PROJECT_NAME} PRIVATE
    "-T${SLOT_LINKER_SCRIPT}"
)

add_subdirectory("${CMAKE_SOURCE_DIR}/../cubemx/cmake/stm32cubemx" "cubemx")

add_subdirectory(../../ymir_api ${CMAKE_BINARY_DIR}/ymir_api)

target_link_libraries(${CMAKE_PROJECT_NAME} ymir_api)

add_custom_command(TARGET ${CMAKE_PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary
        $<TARGET_FILE:${CMAKE_PROJECT_NAME}>
        $<TARGET_FILE_DIR:${CMAKE_PROJECT_NAME}>/${CMAKE_PROJECT_NAME}.bin
    COMMENT "Generating ${CMAKE_PROJECT_NAME}.bin"
)
add_custom_command(TARGET ${CMAKE_PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:${CMAKE_PROJECT_NAME}>
)
