#include "ymir/protocol.hpp"
#include "ymir/crc32.hpp"
#include "ymir/dfu_led.hpp"
#include "ymir/flash.hpp"
#include "ymir/flash_hw.hpp"
#include "ymir/image.hpp"
#include "ymir/metadata.hpp"

#include <array>
#include <cstring>
#include <expected>
#include <span>

namespace ymir {

static constexpr uint8_t SOF = 0xAAu;

static constexpr uint8_t CMD_START  = 0x01u;
static constexpr uint8_t CMD_CHUNK  = 0x02u;
static constexpr uint8_t CMD_FINISH = 0x03u;
static constexpr uint8_t CMD_ABORT  = 0x04u;
static constexpr uint8_t CMD_STATUS = 0x05u;
static constexpr uint8_t CMD_BOOT   = 0x06u;

static constexpr uint8_t RESP_ACK = 0x06u;
static constexpr uint8_t RESP_NAK = 0x15u;

static constexpr uint8_t NAK_BAD_CRC   = 0x01u;
static constexpr uint8_t NAK_BAD_OFFS  = 0x02u;
static constexpr uint8_t NAK_FLASH_ERR = 0x03u;
static constexpr uint8_t NAK_BAD_STATE = 0x04u;

static constexpr std::array<uint8_t, 2> SLOT_FIRST_SEC = {4u, 6u};
static constexpr std::array<uint8_t, 2> SLOT_LAST_SEC  = {5u, 7u};
static constexpr uint32_t               MAX_CHUNK      = 256u;

enum class recv_error_t : uint8_t { TIMEOUT, BAD_CRC };

struct frame_t {
    uint8_t                    cmd;
    std::span<const std::byte> payload;
};

static std::expected<frame_t, recv_error_t> recv_frame(transport_t& t, std::span<std::byte> buf,
                                                       uint32_t timeout_ms)
{
    auto sof = t.rx_byte(timeout_ms);
    if (!sof || *sof != SOF) {
        return std::unexpected(recv_error_t::TIMEOUT);
    }

    auto cmd_b  = t.rx_byte(2000);
    auto len_lo = t.rx_byte(2000);
    auto len_hi = t.rx_byte(2000);
    if (!cmd_b || !len_lo || !len_hi) {
        return std::unexpected(recv_error_t::TIMEOUT);
    }

    uint16_t len = static_cast<uint16_t>(*len_lo) | (static_cast<uint16_t>(*len_hi) << 8);
    if (len > buf.size()) {
        return std::unexpected(recv_error_t::TIMEOUT); // sanity
    }

    for (uint16_t i = 0; i < len; i++) {
        auto b = t.rx_byte(2000);
        if (!b) {
            return std::unexpected(recv_error_t::TIMEOUT);
        }
        buf[i] = std::byte{*b};
    }

    std::array<uint8_t, 4> crc_b;
    for (int i = 0; i < 4; i++) {
        auto b = t.rx_byte(2000);
        if (!b) {
            return std::unexpected(recv_error_t::TIMEOUT);
        }
        crc_b[i] = *b;
    }
    uint32_t recv_crc = static_cast<uint32_t>(crc_b[0]) | (static_cast<uint32_t>(crc_b[1]) << 8) |
                        (static_cast<uint32_t>(crc_b[2]) << 16) |
                        (static_cast<uint32_t>(crc_b[3]) << 24);

    // CRC covers CMD + LEN_LO + LEN_HI + PAYLOAD.
    std::array<uint8_t, 3 + MAX_CHUNK + 4> crc_input;
    crc_input[0] = *cmd_b;
    crc_input[1] = *len_lo;
    crc_input[2] = *len_hi;
    std::memcpy(crc_input.data() + 3, buf.data(), len);
    if (crc32_mpeg2({crc_input.data(), 3u + len}) != recv_crc) {
        return std::unexpected(recv_error_t::BAD_CRC);
    }

    return frame_t{*cmd_b, buf.first(len)};
}

static int send_frame(transport_t& t, uint8_t cmd, const uint8_t* payload, uint16_t plen)
{
    std::array<uint8_t, 8 + MAX_CHUNK + 4> frame;
    frame[0] = SOF;
    frame[1] = cmd;
    frame[2] = static_cast<uint8_t>(plen & 0xFFu);
    frame[3] = static_cast<uint8_t>(plen >> 8);
    if (plen > 0) {
        std::memcpy(frame.data() + 4, payload, plen);
    }
    std::array<uint8_t, 3 + MAX_CHUNK + 4> crc_input;
    crc_input[0] = cmd;
    crc_input[1] = frame[2];
    crc_input[2] = frame[3];
    std::memcpy(crc_input.data() + 3, payload, plen);
    uint32_t crc    = crc32_mpeg2({crc_input.data(), 3u + plen});
    frame[4 + plen] = static_cast<uint8_t>(crc & 0xFFu);
    frame[5 + plen] = static_cast<uint8_t>(crc >> 8);
    frame[6 + plen] = static_cast<uint8_t>(crc >> 16);
    frame[7 + plen] = static_cast<uint8_t>(crc >> 24);
    return t.tx_buf({reinterpret_cast<const std::byte*>(frame.data()), 8u + plen}) ? 0 : -1;
}

static int send_ack(transport_t& t) { return send_frame(t, RESP_ACK, nullptr, 0); }

static int send_nak(transport_t& t, uint8_t reason) { return send_frame(t, RESP_NAK, &reason, 1); }

enum class state_t : uint8_t { IDLE, TRANSFER };

struct xfer_state_t {
    uint8_t  slot;
    uint32_t size;   // total image size
    uint32_t offset; // next expected byte offset
};

static int handle_start(transport_t& t, const std::byte* payload, uint16_t plen, state_t& state,
                        xfer_state_t& xfer)
{
    if (plen < 5) {
        return send_nak(t, NAK_BAD_STATE);
    }
    uint8_t slot = std::to_integer<uint8_t>(payload[0]);
    if (slot > 1) {
        return send_nak(t, NAK_BAD_STATE);
    }
    uint32_t size;
    std::memcpy(&size, payload + 1, 4);
    if (size < 128 || size > 192u * 1024u) {
        return send_nak(t, NAK_BAD_STATE);
    }

    // Erase target slot.
    for (uint8_t s = SLOT_FIRST_SEC[slot]; s <= SLOT_LAST_SEC[slot]; s++) {
        if (!flash::erase_sector(s)) {
            return send_nak(t, NAK_FLASH_ERR);
        }
    }

    // Record transfer start in metadata.
    auto m            = metadata::read().value_or(metadata::metadata_t{});
    m.transfer_slot   = slot;
    m.transfer_size   = size;
    m.transfer_offset = 0;
    m.generation++;
    metadata::write(m);

    xfer  = {slot, size, 0};
    state = state_t::TRANSFER;
    return send_ack(t);
}

static int handle_chunk(transport_t& t, const std::byte* payload, uint16_t plen, state_t& state,
                        xfer_state_t& xfer)
{
    if (state != state_t::TRANSFER) {
        return send_nak(t, NAK_BAD_STATE);
    }
    if (plen < 5) {
        return send_nak(t, NAK_BAD_STATE);
    }

    uint32_t offset;
    std::memcpy(&offset, payload, 4);
    if (offset != xfer.offset) {
        return send_nak(t, NAK_BAD_OFFS);
    }

    uint16_t data_len = plen - 4;
    if (offset + data_len > xfer.size) {
        return send_nak(t, NAK_BAD_STATE);
    }

    uintptr_t addr = slot_base(xfer.slot) + offset;
    if (!flash::write(addr, {payload + 4u, data_len})) {
        return send_nak(t, NAK_FLASH_ERR);
    }

    xfer.offset += data_len;
    return send_ack(t);
}

static int handle_finish(transport_t& t, state_t& state, xfer_state_t& xfer)
{
    if (state != state_t::TRANSFER) {
        return send_nak(t, NAK_BAD_STATE);
    }
    if (xfer.offset != xfer.size) {
        return send_nak(t, NAK_BAD_STATE);
    }

    if (!image::validate(slot_base(xfer.slot))) {
        return send_nak(t, NAK_FLASH_ERR);
    }

    // Clear transfer fields.
    auto m            = metadata::read().value_or(metadata::metadata_t{});
    m.transfer_slot   = 0xFFu;
    m.transfer_size   = 0;
    m.transfer_offset = 0;
    m.generation++;
    metadata::write(m);

    state = state_t::IDLE;
    return send_ack(t);
}

static int handle_status(transport_t& t)
{
    auto m = metadata::read().value_or(metadata::metadata_t{});
    std::array<uint8_t, sizeof(metadata::metadata_t)> buf;
    std::memcpy(buf.data(), &m, sizeof(m));
    return send_frame(t, CMD_STATUS, buf.data(), static_cast<uint16_t>(buf.size()));
}

int protocol_run(transport_t& t)
{
    state_t                              state = state_t::IDLE;
    xfer_state_t                         xfer{};
    std::array<std::byte, MAX_CHUNK + 4> payload_buf;

    for (;;) {
        auto result = recv_frame(t, payload_buf, 200);
        if (!result) {
            if (result.error() == recv_error_t::BAD_CRC) {
                send_nak(t, NAK_BAD_CRC);
            } else {
                dfu_led_update();
            }
            continue;
        }

        const auto& [cmd, payload] = *result;
        switch (cmd) {
        case CMD_START:
            handle_start(t, payload.data(), static_cast<uint16_t>(payload.size()), state, xfer);
            break;
        case CMD_CHUNK:
            handle_chunk(t, payload.data(), static_cast<uint16_t>(payload.size()), state, xfer);
            break;
        case CMD_FINISH:
            handle_finish(t, state, xfer);
            break;
        case CMD_ABORT:
            state = state_t::IDLE;
            xfer  = {};
            send_ack(t);
            break;
        case CMD_STATUS:
            handle_status(t);
            break;
        case CMD_BOOT: {
            if (payload.empty()) {
                send_nak(t, NAK_BAD_STATE);
                break;
            }
            uint8_t slot = std::to_integer<uint8_t>(payload[0]);
            if (slot > 1 || !image::validate(slot_base(slot))) {
                send_nak(t, NAK_BAD_STATE);
                break;
            }
            send_ack(t);
            return static_cast<int>(slot);
        }
        default:
            send_nak(t, NAK_BAD_STATE);
            break;
        }
    }
}

} // namespace ymir
