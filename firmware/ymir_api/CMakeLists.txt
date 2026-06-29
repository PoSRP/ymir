add_library(ymir_api STATIC
    src/api.cpp
    src/platform_hw.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../ymir/src/metadata.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../ymir/src/crc32.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../ymir/src/flash.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../ymir/src/flash_hw.cpp
)

target_include_directories(ymir_api PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../ymir/include
)
