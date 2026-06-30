extern "C" {
#include "stm32f4xx_hal.h"
#include "usbd_cdc.h"
#include "usbd_cdc_if.h"
#include "usbd_def.h"
extern USBD_HandleTypeDef hUsbDeviceFS;
extern IWDG_HandleTypeDef hiwdg;
}

#include "ymir/usb_transport.hpp"

#include <array>

namespace ymir {

// SPSC ring buffer - ISR writes (head), task reads (tail)
static constexpr size_t                          RX_BUF_SIZE = 512;
static std::array<volatile uint8_t, RX_BUF_SIZE> s_rx_buf;
static volatile uint16_t                         s_rx_head = 0;
static volatile uint16_t                         s_rx_tail = 0;

extern "C" void ymir_usb_transport_rx_callback(const uint8_t* buf, uint32_t len)
{
    for (uint32_t i = 0; i < len; i++) {
        uint16_t next = (s_rx_head + 1) % RX_BUF_SIZE;
        if (next == s_rx_tail) {
            break; // buffer full, drop data
        }
        s_rx_buf[s_rx_head] = buf[i];
        __DMB();
        s_rx_head = next;
    }
}

std::optional<uint8_t> usb_transport_t::rx_byte(uint32_t timeout_ms)
{
    uint32_t start = HAL_GetTick();
    while (HAL_GetTick() - start < timeout_ms) {
        if (s_rx_head != s_rx_tail) {
            uint8_t b = s_rx_buf[s_rx_tail];
            __DMB();
            s_rx_tail = (s_rx_tail + 1) % RX_BUF_SIZE;
            return b;
        }
        HAL_IWDG_Refresh(&hiwdg);
        HAL_Delay(1);
    }
    return std::nullopt;
}

bool usb_transport_t::tx_buf(std::span<const std::byte> buf)
{
    auto*    data  = reinterpret_cast<const uint8_t*>(buf.data());
    uint32_t start = HAL_GetTick();
    // CDC_Transmit_FS takes uint8_t* but does not modify the buffer.
    while (CDC_Transmit_FS(const_cast<uint8_t*>(data), static_cast<uint16_t>(buf.size())) ==
           USBD_BUSY) {
        if (HAL_GetTick() - start > 1000) {
            return false;
        }
        HAL_IWDG_Refresh(&hiwdg);
        HAL_Delay(1);
    }
    start      = HAL_GetTick();
    auto* hcdc = static_cast<USBD_CDC_HandleTypeDef*>(hUsbDeviceFS.pClassData);
    while (hcdc->TxState != 0) {
        if (HAL_GetTick() - start > 1000) {
            return false;
        }
        HAL_IWDG_Refresh(&hiwdg);
        HAL_Delay(1);
    }
    return true;
}

} // namespace ymir
