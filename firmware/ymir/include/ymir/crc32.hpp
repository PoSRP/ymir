#ifndef YMIR_CRC32_HPP
#define YMIR_CRC32_HPP

#include <cstddef>
#include <cstdint>
#include <span>

namespace ymir {

/**
 * CRC-32/MPEG-2
 * poly=0x04C11DB7
 * init=0xFFFFFFFF
 * no reflection
 * no final XOR
 */
uint32_t crc32_mpeg2(std::span<const uint8_t> data);

} // namespace ymir

#endif // YMIR_CRC32_HPP
