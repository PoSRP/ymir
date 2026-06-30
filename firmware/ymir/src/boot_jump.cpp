#include "stm32f4xx.h"

#include <cstdint>
#include <utility>

namespace ymir {

/**
 * Disables SysTick and all NVIC interrupts, relocates the vector table, then
 * loads the target's initial SP and jumps to its reset handler. The CPU state
 * must be clean so pending bootloader interrupts don't fire in the application.
 */
void boot_jump(uint32_t vt_base)
{
    SysTick->CTRL = 0;

    for (int i = 0; i < 8; i++) {
        NVIC->ICER[i] = 0xFFFFFFFFu;
        NVIC->ICPR[i] = 0xFFFFFFFFu;
    }

    __disable_irq();
    __DSB();
    __ISB();

    SCB->VTOR = vt_base;
    __DSB();

    uint32_t sp    = *reinterpret_cast<const volatile uint32_t*>(vt_base);
    uint32_t entry = *reinterpret_cast<const volatile uint32_t*>(vt_base + 4u);

    __set_CONTROL(0U);
    __ISB();
    __set_BASEPRI(0U);
    __ISB();
    __enable_irq();

    asm volatile("msr msp, %0\n"
                 "bx  %1\n"
                 :
                 : "r"(sp), "r"(entry));

    std::unreachable();
}

} // namespace ymir
