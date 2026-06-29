#include "main.h"

#include "ymir/api.h"

#include <array>
#include <cstdint>
#include <cstring>
#include <span>

static constexpr uint32_t                        RX_BUF_SIZE = 512;
static std::array<volatile uint8_t, RX_BUF_SIZE> rx_buf;
static volatile uint32_t                         rx_head = 0;
static volatile uint32_t                         rx_tail = 0;

extern "C" void app_usb_rx(const uint8_t* buf, uint32_t len)
{
    for (uint32_t i = 0; i < len; i++) {
        uint32_t next = (rx_head + 1) % RX_BUF_SIZE;
        if (next != rx_tail) {
            rx_buf[rx_head] = buf[i];
            __DMB();
            rx_head = next;
        }
    }
}

static bool rx_read(uint8_t* out)
{
    if (rx_tail == rx_head) {
        return false;
    }
    *out = rx_buf[rx_tail];
    __DMB();
    rx_tail = (rx_tail + 1) % RX_BUF_SIZE;
    return true;
}

extern "C" void cpp_main(void)
{
    ymir::confirm_boot();

    unsigned delay = static_cast<unsigned>(ymir::current_slot()) * 250u;

    std::array<uint8_t, 8> dfu_buf{};
    uint32_t               dfu_pos = 0;

    for (;;) {
        HAL_GPIO_TogglePin(USER_LED_GPIO_Port, USER_LED_Pin);
        HAL_Delay(delay);

        uint8_t b;
        while (rx_read(&b)) {
            dfu_buf[dfu_pos++] = b;
            if (dfu_pos == 8) {
                if (ymir::is_enter_update_request(std::span<const uint8_t, 8>(dfu_buf))) {
                    ymir::enter_update();
                }
                std::memmove(dfu_buf.data(), dfu_buf.data() + 1, 7);
                dfu_pos = 7;
            }
        }
    }
}
