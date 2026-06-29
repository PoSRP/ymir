#include "ymir/crc32.hpp"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("crc32 known value", "[crc32]")
{
    const uint8_t data[] = {0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39};
    REQUIRE(ymir::crc32_mpeg2({data, sizeof(data)}) == 0x0376E6E7u);
}

TEST_CASE("crc32 empty", "[crc32]") { REQUIRE(ymir::crc32_mpeg2({}) == 0xFFFFFFFFu); }
