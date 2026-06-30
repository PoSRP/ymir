#ifndef YMIR_METADATA_HPP
#define YMIR_METADATA_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace ymir::metadata {

struct metadata_t {
    uint32_t                 magic;
    uint32_t                 generation;
    uint8_t                  active_slot;
    uint8_t                  boot_count;
    uint8_t                  confirmed;
    uint8_t                  transfer_slot;
    uint32_t                 transfer_offset;
    uint32_t                 transfer_size;
    std::array<std::byte, 8> _reserved;
    uint32_t                 crc32;
} __attribute__((packed));

static_assert(sizeof(metadata_t) == 32, "metadata_t must be 32 bytes");

std::optional<metadata_t> read();

void write(const metadata_t& m);

} // namespace ymir::metadata

#endif // YMIR_METADATA_HPP
