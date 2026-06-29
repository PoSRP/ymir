extern "C" {
#include "stm32f4xx_hal.h"
void MX_USB_DEVICE_Init(void);
}

#include "ymir/boot_jump.hpp"
#include "ymir/flash_layout.hpp"
#include "ymir/image.hpp"
#include "ymir/metadata.hpp"
#include "ymir/protocol.hpp"
#include "ymir/usb_transport.hpp"

#include <array>

namespace ymir {

static const std::array<uintptr_t, 2> SLOT_BASE = {
    reinterpret_cast<uintptr_t>(&_slot_a_base),
    reinterpret_cast<uintptr_t>(&_slot_b_base),
};
static const std::array<uintptr_t, 2> SLOT_VT = {
    reinterpret_cast<uintptr_t>(&_slot_a_base) + sizeof(image::header_t),
    reinterpret_cast<uintptr_t>(&_slot_b_base) + sizeof(image::header_t),
};

/**
 * Top word of SRAM, outside the linker's RAM region so the stack never
 *   touches it. Survives SYSRESETREQ.
 */
static constexpr uint32_t BOOT_FLAG_ADDR = 0x2001FFFFu - 3u;
static constexpr uint32_t DFU_MAGIC      = 0xB007DA7Au;

void boot_manager_run(void)
{
    auto* boot_flag = reinterpret_cast<volatile uint32_t*>(BOOT_FLAG_ADDR);

    if (*boot_flag == DFU_MAGIC) {
        *boot_flag = 0;
        MX_USB_DEVICE_Init();
        usb_transport_t usb;
        int             boot_slot = protocol_run(usb);
        if (boot_slot >= 0) {
            auto m        = metadata::read().value_or(metadata::metadata_t{});
            m.active_slot = static_cast<uint8_t>(boot_slot);
            m.boot_count  = 1;
            m.confirmed   = 0;
            m.generation++;
            metadata::write(m);
        }
        HAL_NVIC_SystemReset();
    }

    auto m = metadata::read().value_or(metadata::metadata_t{
        .magic           = 0xBAADF00Du,
        .generation      = 0,
        .active_slot     = 0,
        .boot_count      = 0,
        .confirmed       = 1, // skip rollback check on first boot
        .transfer_slot   = 0xFF,
        .transfer_offset = 0,
        .transfer_size   = 0,
    });

    // Rollback: too many unconfirmed boots on this slot -> try the other
    if (m.boot_count >= 3 && !m.confirmed) {
        m.active_slot ^= 1;
        m.boot_count = 0;
        m.confirmed  = 0;
        m.generation++;
        metadata::write(m);
    }

    // Slot validation with automatic fallback
    if (!image::validate(SLOT_BASE[m.active_slot])) {
        uint8_t other = m.active_slot ^ 1;
        if (!image::validate(SLOT_BASE[other])) {
            *boot_flag = DFU_MAGIC;
            HAL_NVIC_SystemReset();
        }
        m.active_slot = other;
        m.boot_count  = 0;
        m.confirmed   = 0;
        m.generation++;
        metadata::write(m);
    }

    // Record this boot attempt, app must call confirm to clear it
    m.boot_count++;
    m.confirmed = 0;
    m.generation++;
    metadata::write(m);

    // This will not return
    boot_jump(SLOT_VT[m.active_slot]);
}

} // namespace ymir
