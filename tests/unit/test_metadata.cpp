#include "ymir/metadata.hpp"

#include <cstring>

#include <catch2/catch_test_macros.hpp>

extern void fake_flash_reset();

static ymir::metadata::metadata_t make(uint32_t gen, uint8_t slot, uint8_t confirmed)
{
    ymir::metadata::metadata_t m{};
    m.magic         = 0xBAADF00Du;
    m.generation    = gen;
    m.active_slot   = slot;
    m.confirmed     = confirmed;
    m.transfer_slot = 0xFF;
    return m;
}

TEST_CASE("metadata read empty returns nullopt", "[metadata]")
{
    fake_flash_reset();
    REQUIRE(!ymir::metadata::read());
}

TEST_CASE("metadata write and read", "[metadata]")
{
    fake_flash_reset();
    ymir::metadata::write(make(1, 0, 1));
    auto out = ymir::metadata::read();
    REQUIRE(out);
    REQUIRE(out->generation == 1);
    REQUIRE(out->active_slot == 0);
    REQUIRE(out->confirmed == 1);
}

TEST_CASE("metadata highest generation wins", "[metadata]")
{
    fake_flash_reset();
    ymir::metadata::write(make(1, 0, 1));
    ymir::metadata::write(make(3, 1, 0));
    ymir::metadata::write(make(2, 0, 1));
    auto out = ymir::metadata::read();
    REQUIRE(out);
    REQUIRE(out->generation == 3);
}

TEST_CASE("metadata corrupt crc falls back to previous", "[metadata]")
{
    fake_flash_reset();
    ymir::metadata::write(make(1, 0, 1));
    ymir::metadata::write(make(2, 1, 0));

    extern uint8_t g_meta_flash[];
    g_meta_flash[32 + 28] ^= 0xFF;

    auto out = ymir::metadata::read();
    REQUIRE(out);
    REQUIRE(out->generation == 1);
}

TEST_CASE("metadata sector fill and wrap", "[metadata]")
{
    fake_flash_reset();
    for (uint32_t i = 1; i <= 512; i++) {
        ymir::metadata::write(make(i, 0, 1));
    }
    ymir::metadata::write(make(513, 1, 0));
    auto out = ymir::metadata::read();
    REQUIRE(out);
    REQUIRE(out->generation == 513);
    REQUIRE(out->active_slot == 1);
}
