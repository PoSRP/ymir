#include "stm32f4xx_hal.h"

#include "ymir/dfu_led.hpp"

namespace ymir {

void dfu_led_update()
{
    uint32_t t  = HAL_GetTick() % 1800;
    bool     on = t < 200 || (t >= 400 && t < 600);
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, on ? GPIO_PIN_RESET : GPIO_PIN_SET);
}

} // namespace ymir
