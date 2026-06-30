#include "ymir/flash_hw.hpp"
#include "ymir/flash_layout.hpp"

#include <array>

namespace ymir {

uintptr_t meta_base() { return reinterpret_cast<uintptr_t>(&_metadata_base); }

uintptr_t slot_base(uint8_t slot)
{
    static const std::array<uintptr_t, 2> bases = {
        reinterpret_cast<uintptr_t>(&_slot_a_base),
        reinterpret_cast<uintptr_t>(&_slot_b_base),
    };
    return bases[slot];
}

uintptr_t slot_flash_base(uint8_t slot) { return slot_base(slot); }

uintptr_t slot_flash_end(uint8_t slot)
{
    static const std::array<uintptr_t, 2> ends = {
        reinterpret_cast<uintptr_t>(&_slot_a_end),
        reinterpret_cast<uintptr_t>(&_slot_b_end),
    };
    return ends[slot];
}

} // namespace ymir
