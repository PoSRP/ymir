#include "ymir/crc32.hpp"

namespace ymir {

uint32_t crc32_mpeg2(std::span<const uint8_t> data)
{
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < data.size(); i++) {
        crc ^= (uint32_t)data[i] << 24;
        for (int j = 0; j < 8; j++) {
            crc = (crc & 0x80000000u) ? (crc << 1) ^ 0x04C11DB7u : crc << 1;
        }
    }
    return crc;
}

} // namespace ymir
