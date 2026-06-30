#include "ymir/flash.hpp"
#include "ymir/flash_hw.hpp"

#include <array>
#include <cstring>
#include <span>

/* Sector geometry (matches STM32F411 layout):
     sector 3 → meta (16 KB)
     sector 4 → slot_a[    0 ..  64K)   (64 KB)
     sector 5 → slot_a[ 64K .. 192K)   (128 KB)
     sector 6 → slot_b[    0 .. 128K)  (128 KB)
     sector 7 → slot_b[128K .. 256K)  (128 KB)
*/
static constexpr size_t META_SECTOR_SIZE = 16u * 1024u;
static constexpr size_t SLOT_A_SIZE      = 192u * 1024u;
static constexpr size_t SLOT_B_SIZE      = 256u * 1024u;

uint8_t g_meta_flash[META_SECTOR_SIZE];
uint8_t g_slot_a_flash[SLOT_A_SIZE];
uint8_t g_slot_b_flash[SLOT_B_SIZE];

namespace ymir {

uintptr_t meta_base() { return reinterpret_cast<uintptr_t>(g_meta_flash); }

uintptr_t slot_base(uint8_t slot)
{
    static const std::array<uintptr_t, 2> bases = {
        reinterpret_cast<uintptr_t>(g_slot_a_flash),
        reinterpret_cast<uintptr_t>(g_slot_b_flash),
    };
    return bases[slot];
}

uintptr_t slot_flash_base(uint8_t slot)
{
    static const std::array<uintptr_t, 2> bases = {0x08010000u, 0x08040000u};
    return bases[slot];
}

uintptr_t slot_flash_end(uint8_t slot)
{
    static const std::array<uintptr_t, 2> ends = {0x08040000u, 0x08080000u};
    return ends[slot];
}

} // namespace ymir

void fake_flash_reset()
{
    memset(g_meta_flash, 0xFF, sizeof(g_meta_flash));
    memset(g_slot_a_flash, 0xFF, sizeof(g_slot_a_flash));
    memset(g_slot_b_flash, 0xFF, sizeof(g_slot_b_flash));
}

bool ymir::flash::erase_sector(uint32_t sector)
{
    switch (sector) {
    case 3:
        memset(g_meta_flash, 0xFF, META_SECTOR_SIZE);
        return true;
    case 4:
        memset(g_slot_a_flash, 0xFF, 64u * 1024u);
        return true;
    case 5:
        memset(g_slot_a_flash + 64u * 1024u, 0xFF, 128u * 1024u);
        return true;
    case 6:
        memset(g_slot_b_flash, 0xFF, 128u * 1024u);
        return true;
    case 7:
        memset(g_slot_b_flash + 128u * 1024u, 0xFF, 128u * 1024u);
        return true;
    default:
        return false;
    }
}

bool ymir::flash::write(uintptr_t addr, std::span<const std::byte> data)
{
    auto region = [&](uint8_t* base, size_t size) -> bool {
        uintptr_t bbase = reinterpret_cast<uintptr_t>(base);
        if (addr < bbase || addr + data.size() > bbase + size) {
            return false;
        }
        std::memcpy(base + (addr - bbase), data.data(), data.size());
        return true;
    };
    uintptr_t meta = reinterpret_cast<uintptr_t>(g_meta_flash);
    uintptr_t a    = reinterpret_cast<uintptr_t>(g_slot_a_flash);
    uintptr_t b    = reinterpret_cast<uintptr_t>(g_slot_b_flash);
    if (addr >= meta && addr < meta + META_SECTOR_SIZE) {
        return region(g_meta_flash, META_SECTOR_SIZE);
    }
    if (addr >= a && addr < a + SLOT_A_SIZE) {
        return region(g_slot_a_flash, SLOT_A_SIZE);
    }
    if (addr >= b && addr < b + SLOT_B_SIZE) {
        return region(g_slot_b_flash, SLOT_B_SIZE);
    }
    return false;
}
