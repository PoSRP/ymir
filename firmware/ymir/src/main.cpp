#include "gpio.h"
#include "iwdg.h"

#include "ymir/boot_manager.hpp"

#include <utility>

extern "C" [[noreturn]] void ymir_main(void)
{
    MX_GPIO_Init();
    MX_IWDG_Init();
    ymir::boot_manager_run();
    std::unreachable();
}
