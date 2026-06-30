#ifndef YMIR_TRANSPORT_HPP
#define YMIR_TRANSPORT_HPP

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace ymir {

struct transport_t {
    virtual std::optional<uint8_t> rx_byte(uint32_t timeout_ms)           = 0;
    virtual bool                   tx_buf(std::span<const std::byte> buf) = 0;
    virtual ~transport_t()                                                = default;
};

} // namespace ymir

#endif // YMIR_TRANSPORT_HPP
