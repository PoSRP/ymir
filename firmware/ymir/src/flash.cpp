#include "ymir/flash.hpp"

#include <array>
#include <cstring>

namespace ymir::flash {

/**
 * Manipulating flash registers directly to avoid imposing a HAL dependency on upstream consumers
 */

static volatile uint32_t* const FLASH_KEYR = reinterpret_cast<volatile uint32_t*>(0x40023C04u);
static volatile uint32_t* const FLASH_SR   = reinterpret_cast<volatile uint32_t*>(0x40023C0Cu);
static volatile uint32_t* const FLASH_CR   = reinterpret_cast<volatile uint32_t*>(0x40023C10u);

static void flash_unlock()
{
    *FLASH_KEYR = 0x45670123u;
    *FLASH_KEYR = 0xCDEF89ABu;
    // Clear any sticky error bits left by a previous operation
    *FLASH_SR = 0xF2u;
}

static void flash_lock() { *FLASH_CR |= (1u << 31); }

static void flash_wait()
{
    while (*FLASH_SR & (1u << 16)) {
    }
}

bool erase_sector(uint32_t sector)
{
    flash_unlock();
    flash_wait();
    *FLASH_CR = (2u << 8) | (1u << 1) | ((uint32_t)sector << 3);
    *FLASH_CR |= (1u << 16);
    flash_wait();
    *FLASH_CR = 0;
    bool ok   = (*FLASH_SR & 0xF2u) == 0;
    flash_lock();
    return ok;
}

bool write(uintptr_t addr, std::span<const std::byte> data)
{
    size_t padded = (data.size() + 3u) & ~3u;
    flash_unlock();
    flash_wait();
    *FLASH_CR = (2u << 8) | (1u << 0);
    for (size_t i = 0; i < padded; i += 4) {
        std::array<uint8_t, 4> buf   = {0xFF, 0xFF, 0xFF, 0xFF};
        size_t                 chunk = (data.size() - i < 4) ? (data.size() - i) : 4;
        std::memcpy(buf.data(), data.data() + i, chunk);
        uint32_t word;
        std::memcpy(&word, buf.data(), 4);
        *reinterpret_cast<volatile uint32_t*>(addr + i) = word;
        flash_wait();
        if (*FLASH_SR & 0xF2u) {
            *FLASH_CR = 0;
            flash_lock();
            return false;
        }
    }
    *FLASH_CR = 0;
    flash_lock();
    return true;
}

} // namespace ymir::flash
