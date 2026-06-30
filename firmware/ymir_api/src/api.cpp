#include "ymir/api.h"
#include "platform.hpp"

#include "ymir/flash_hw.hpp"
#include "ymir/metadata.hpp"
#include "ymir/crc32.hpp"

#include <array>
#include <bit>
#include <cstring>
#include <span>

static constexpr uint32_t DFU_MAGIC      = 0xB007DA7Au;
static constexpr uint32_t AIRCR_SYSRESET = 0x05FA0004u;

static constexpr std::array<uint8_t, 4> DFU_MAGIC_ARR = {0xADu, 0xF0u, 0x00u, 0x00u};

extern "C" int ymir_is_enter_update_request(const uint8_t* buf)
{
    std::span<const uint8_t, 8> header{buf, 8};
    if (std::memcmp(header.data(), DFU_MAGIC_ARR.data(), DFU_MAGIC_ARR.size()) != 0) {
        return 0;
    }
    uint32_t stored;
    std::memcpy(&stored, header.data() + 4, 4);
    return ymir::crc32_mpeg2(header.first(4)) == std::byteswap(stored) ? 1 : 0;
}

extern "C" int ymir_current_slot(void)
{
    uint32_t vtor = platform_read_vtor();
    if (vtor >= ymir::slot_flash_base(0) && vtor < ymir::slot_flash_end(0)) {
        return 1;
    }
    if (vtor >= ymir::slot_flash_base(1) && vtor < ymir::slot_flash_end(1)) {
        return 2;
    }
    return 0;
}

extern "C" void ymir_confirm_boot(void)
{
    platform_iwdg_feed();
    auto m_opt = ymir::metadata::read();
    if (!m_opt) {
        return;
    }
    auto m = *m_opt;
    if (m.confirmed) {
        return;
    }
    m.confirmed  = 1;
    m.boot_count = 0;
    m.generation++;
    ymir::metadata::write(m);
    platform_iwdg_feed();
}

extern "C" void ymir_enter_update(void)
{
    platform_write_dfu_flag(DFU_MAGIC);
    platform_write_aircr(AIRCR_SYSRESET);
    platform_hang();
}

extern "C" void ymir_feed_watchdog(void) { platform_iwdg_feed(); }

extern "C" void ymir_request_rollback(void)
{
    auto m_opt = ymir::metadata::read();
    if (!m_opt) {
        return;
    }
    auto m       = *m_opt;
    m.confirmed  = 0;
    m.boot_count = 3;
    m.generation++;
    ymir::metadata::write(m);
    platform_write_aircr(AIRCR_SYSRESET);
    platform_hang();
}
