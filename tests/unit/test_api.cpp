#include "ymir/api.h"
#include "ymir/metadata.hpp"
#include "ymir/crc32.hpp"

#include <catch2/catch_test_macros.hpp>

extern void         fake_flash_reset();
extern "C" uint32_t g_test_vtor;
extern "C" uint32_t g_test_dfu_flag;
extern "C" uint32_t g_test_aircr;
extern "C" uint32_t g_test_iwdg_kr;

static constexpr uint32_t DFU_MAGIC   = 0xB007DA7Au;
static constexpr uint32_t AIRCR_RESET = 0x05FA0004u;

static void seed_metadata(uint8_t slot, uint8_t boot_count, uint8_t confirmed)
{
    ymir::metadata::metadata_t m{};
    m.magic         = 0xBAADF00Du;
    m.generation    = 7;
    m.active_slot   = slot;
    m.boot_count    = boot_count;
    m.confirmed     = confirmed;
    m.transfer_slot = 0xFF;
    ymir::metadata::write(m);
}

static std::array<uint8_t, 8> make_valid_header()
{
    std::array<uint8_t, 8> h{0xAD, 0xF0, 0x00, 0x00, 0, 0, 0, 0};
    uint32_t               crc = ymir::crc32_mpeg2({h.data(), 4u});
    h[4]                       = static_cast<uint8_t>(crc >> 24);
    h[5]                       = static_cast<uint8_t>(crc >> 16);
    h[6]                       = static_cast<uint8_t>(crc >> 8);
    h[7]                       = static_cast<uint8_t>(crc);
    return h;
}

TEST_CASE("ymir_is_enter_update_request valid", "[dfu]")
{
    REQUIRE(ymir_is_enter_update_request(make_valid_header().data()));
}

TEST_CASE("ymir_is_enter_update_request wrong magic", "[dfu]")
{
    auto h = make_valid_header();
    h[0] ^= 0xFF;
    REQUIRE(!ymir_is_enter_update_request(h.data()));
}

TEST_CASE("ymir_is_enter_update_request wrong type", "[dfu]")
{
    auto h = make_valid_header();
    h[1] ^= 0x01;
    REQUIRE(!ymir_is_enter_update_request(h.data()));
}

TEST_CASE("ymir_is_enter_update_request nonzero len", "[dfu]")
{
    auto h = make_valid_header();
    h[3]   = 0x01;
    REQUIRE(!ymir_is_enter_update_request(h.data()));
}

TEST_CASE("ymir_is_enter_update_request bad crc", "[dfu]")
{
    auto h = make_valid_header();
    h[7] ^= 0x01;
    REQUIRE(!ymir_is_enter_update_request(h.data()));
}

TEST_CASE("ymir_current_slot", "[bootloader_api]")
{
    g_test_vtor = 0x08000000;
    REQUIRE(ymir_current_slot() == 0);
    g_test_vtor = 0x08010200;
    REQUIRE(ymir_current_slot() == 1);
    g_test_vtor = 0x0803FFFF;
    REQUIRE(ymir_current_slot() == 1);
    g_test_vtor = 0x08040200;
    REQUIRE(ymir_current_slot() == 2);
    g_test_vtor = 0x0807FFFF;
    REQUIRE(ymir_current_slot() == 2);
}

TEST_CASE("ymir_confirm_boot sets confirmed flag", "[bootloader_api]")
{
    fake_flash_reset();
    g_test_iwdg_kr = 0;
    seed_metadata(0, 2, 0);

    ymir_confirm_boot();

    auto m = ymir::metadata::read();
    REQUIRE(m);
    REQUIRE(m->confirmed == 1);
    REQUIRE(m->boot_count == 0);
    REQUIRE(m->generation == 8);
    REQUIRE(g_test_iwdg_kr == 0xAAAA);
}

TEST_CASE("ymir_confirm_boot is idempotent when already confirmed", "[bootloader_api]")
{
    fake_flash_reset();
    seed_metadata(0, 0, 1);

    ymir_confirm_boot();

    auto m = ymir::metadata::read();
    REQUIRE(m);
    REQUIRE(m->generation == 7);
}

TEST_CASE("ymir_confirm_boot is noop without metadata", "[bootloader_api]")
{
    fake_flash_reset();
    g_test_iwdg_kr = 0;

    ymir_confirm_boot();

    REQUIRE(!ymir::metadata::read());
    REQUIRE(g_test_iwdg_kr == 0xAAAA);
}

TEST_CASE("ymir_feed_watchdog reloads IWDG", "[bootloader_api]")
{
    g_test_iwdg_kr = 0;

    ymir_feed_watchdog();

    REQUIRE(g_test_iwdg_kr == 0xAAAA);
}

TEST_CASE("ymir_request_rollback sets boot_count and resets", "[bootloader_api]")
{
    fake_flash_reset();
    g_test_aircr = 0;
    seed_metadata(0, 0, 1);

    ymir_request_rollback();

    auto m = ymir::metadata::read();
    REQUIRE(m);
    REQUIRE(m->confirmed == 0);
    REQUIRE(m->boot_count == 3);
    REQUIRE(m->generation == 8);
    REQUIRE(g_test_aircr == AIRCR_RESET);
}

TEST_CASE("ymir_request_rollback is noop without metadata", "[bootloader_api]")
{
    fake_flash_reset();
    g_test_aircr = 0;

    ymir_request_rollback();

    REQUIRE(g_test_aircr == 0);
}

TEST_CASE("ymir_enter_update sets dfu flag and resets", "[bootloader_api]")
{
    g_test_dfu_flag = 0;
    g_test_aircr    = 0;

    ymir_enter_update();

    REQUIRE(g_test_dfu_flag == DFU_MAGIC);
    REQUIRE(g_test_aircr == AIRCR_RESET);
}
