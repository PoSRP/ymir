#ifndef YMIR_USB_TRANSPORT_HPP
#define YMIR_USB_TRANSPORT_HPP

#include "ymir/transport.hpp"

namespace ymir {

class usb_transport_t : public transport_t {
public:
    std::optional<uint8_t> rx_byte(uint32_t timeout_ms) override;
    bool                   tx_buf(std::span<const std::byte> buf) override;
};

} // namespace ymir

// Called from CDC_Receive_FS (USB interrupt context)
extern "C" void ymir_usb_transport_rx_callback(const uint8_t* buf, uint32_t len);

#endif // YMIR_USB_TRANSPORT_HPP
