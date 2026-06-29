#ifndef YMIR_FLASH_HPP
#define YMIR_FLASH_HPP

#include <cstddef>
#include <cstdint>
#include <span>

namespace ymir::flash {

bool erase_sector(uint32_t sector);

bool write(uintptr_t addr, std::span<const std::byte> data);

} // namespace ymir::flash

#endif // YMIR_FLASH_HPP
