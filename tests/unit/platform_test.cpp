#include "platform.hpp"

#include <cstdint>

extern "C" {
uint32_t g_test_vtor     = 0;
uint32_t g_test_dfu_flag = 0;
uint32_t g_test_aircr    = 0;
uint32_t g_test_iwdg_kr  = 0;
}

uint32_t platform_read_vtor() { return g_test_vtor; }
void     platform_write_dfu_flag(uint32_t v) { g_test_dfu_flag = v; }
void     platform_write_aircr(uint32_t v) { g_test_aircr = v; }
void     platform_iwdg_feed() { g_test_iwdg_kr = 0xAAAAu; }
void     platform_hang() {}
