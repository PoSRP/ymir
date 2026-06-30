#include "platform.hpp"

static constexpr uint32_t DFU_FLAG_ADDR = 0x2001FFFFu - 3u;

uint32_t platform_read_vtor() { return *reinterpret_cast<const volatile uint32_t*>(0xE000ED08u); }

void platform_write_dfu_flag(uint32_t v)
{
    *reinterpret_cast<volatile uint32_t*>(DFU_FLAG_ADDR) = v;
    __asm volatile("dsb" ::: "memory");
}

void platform_write_aircr(uint32_t v) { *reinterpret_cast<volatile uint32_t*>(0xE000ED0Cu) = v; }

void platform_iwdg_feed() { *reinterpret_cast<volatile uint32_t*>(0x40003000u) = 0xAAAAu; }

void platform_hang()
{
    for (;;) {
    }
}
