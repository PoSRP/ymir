#include "ymir/image.hpp"
#include "ymir/sha256.hpp"

#include <array>
#include <cstddef>
#include <cstring>

namespace ymir {

static constexpr uint32_t IMAGE_MAGIC    = 0xB007AB1Eu;
static constexpr uint32_t MAX_IMAGE_SIZE = 192u * 1024u;

bool image::validate(uintptr_t slot_base)
{
    const auto* h = reinterpret_cast<const header_t*>(slot_base);
    if (h->magic != IMAGE_MAGIC) {
        return false;
    }
    if (h->flags != 0) {
        return false;
    }
    if (h->image_size <= 512 || h->image_size > MAX_IMAGE_SIZE) {
        return false;
    }

    std::array<std::byte, 32> digest;
    sha256({reinterpret_cast<const std::byte*>(slot_base + 512), h->image_size - 512}, digest);
    return std::memcmp(digest.data(), h->sha256.data(), 32) == 0;
}

} // namespace ymir
