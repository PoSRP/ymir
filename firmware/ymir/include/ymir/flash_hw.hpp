#ifndef YMIR_FLASH_HW_HPP
#define YMIR_FLASH_HW_HPP

#include <cstdint>

namespace ymir {

uintptr_t meta_base();

uintptr_t slot_base(uint8_t slot);

uintptr_t slot_flash_base(uint8_t slot);

uintptr_t slot_flash_end(uint8_t slot);

} // namespace ymir

#endif // YMIR_FLASH_HW_HPP
