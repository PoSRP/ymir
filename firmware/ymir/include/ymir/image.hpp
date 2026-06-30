#ifndef YMIR_IMAGE_HPP
#define YMIR_IMAGE_HPP

#include <array>
#include <cstddef>
#include <cstdint>

namespace ymir::image {

struct header_t {
    uint32_t                   magic;
    uint32_t                   version;
    uint32_t                   image_size;
    uint32_t                   flags;
    std::array<std::byte, 32>  sha256;
    uint8_t                    slot;
    std::array<std::byte, 463> _reserved;
} __attribute__((packed));

static_assert(sizeof(header_t) == 512, "image::header_t must be 512 bytes");

bool validate(uintptr_t slot_base);

} // namespace ymir::image

#endif // YMIR_IMAGE_HPP
