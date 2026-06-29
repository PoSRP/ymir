#ifndef YMIR_API_PLATFORM_HPP
#define YMIR_API_PLATFORM_HPP

#include <cstdint>

uint32_t platform_read_vtor();
void     platform_write_dfu_flag(uint32_t v);
void     platform_write_aircr(uint32_t v);
void     platform_iwdg_feed();
void     platform_hang();

#endif // YMIR_API_PLATFORM_HPP
