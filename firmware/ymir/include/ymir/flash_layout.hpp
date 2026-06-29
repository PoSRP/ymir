#ifndef YMIR_FLASH_LAYOUT_HPP
#define YMIR_FLASH_LAYOUT_HPP

// Linker-defined flash region boundaries (firmware/linker/*.ld).
// Usage: reinterpret_cast<uintptr_t>(&_slot_a_base)
extern const char _metadata_base;
extern const char _slot_a_base;
extern const char _slot_a_end;
extern const char _slot_b_base;
extern const char _slot_b_end;

#endif // YMIR_FLASH_LAYOUT_HPP
