cmake_minimum_required(VERSION 3.22)
project(unit_tests CXX)

find_package(Catch2 3 REQUIRED)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(FW_SRC "/firmware/ymir/src" CACHE PATH "Bootloader source directory")
set(FW_INC "/firmware/ymir/include" CACHE PATH "Bootloader include directory")
set(BL_API_SRC "/firmware/ymir_api/src" CACHE PATH "Bootloader API source directory")
set(BL_INC "/firmware/include" CACHE PATH "Bootloader public include directory")

add_executable(unit_tests
    "${FW_SRC}/crc32.cpp"
    "${FW_SRC}/sha256.cpp"
    "${FW_SRC}/metadata.cpp"
    "${FW_SRC}/image.cpp"
    "${FW_SRC}/protocol.cpp"
    "${BL_API_SRC}/api.cpp"
    fake_flash.cpp
    platform_test.cpp
    dfu_led_test.cpp
    test_crc32.cpp
    test_sha256.cpp
    test_image.cpp
    test_metadata.cpp
    test_api.cpp
    test_protocol.cpp
)

target_include_directories(unit_tests PRIVATE "${FW_SRC}" "${FW_INC}" "${BL_API_SRC}" "${BL_INC}")
target_compile_options(unit_tests PRIVATE -Wall -Wextra -Werror)
target_link_libraries(unit_tests PRIVATE Catch2::Catch2WithMain)

# Round-trip helper used by tests/unit/test_prepare_image.py:
# reads an .img file from argv[1], runs Image::validate, exits 0/1.
add_executable(validate_image_cli
    "${FW_SRC}/sha256.cpp"
    "${FW_SRC}/image.cpp"
    validate_image_cli.cpp
)
target_include_directories(validate_image_cli PRIVATE "${FW_INC}")
target_compile_options(validate_image_cli PRIVATE -Wall -Wextra -Werror)
