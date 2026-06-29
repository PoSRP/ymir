#include "ymir/crc32.hpp"
#include "ymir/image.hpp"
#include "ymir/metadata.hpp"
#include "ymir/protocol.hpp"
#include "ymir/sha256.hpp"
#include "ymir/transport.hpp"

#include <algorithm>
#include <cstring>
#include <vector>

#include <catch2/catch_test_macros.hpp>

extern void    fake_flash_reset();
extern uint8_t g_slot_a_flash[];

/* ── Frame helpers ──────────────────────────────────────────────────────── */

static constexpr uint8_t SOF = 0xAAu;

static constexpr uint8_t CMD_START        = 0x01u;
static constexpr uint8_t CMD_CHUNK        = 0x02u;
static constexpr uint8_t CMD_FINISH       = 0x03u;
static constexpr uint8_t CMD_ABORT        = 0x04u;
static constexpr uint8_t CMD_STATUS       = 0x05u;
static constexpr uint8_t CMD_BOOT         = 0x06u;
static constexpr uint8_t CMD_CONFIRM      = 0x07u;
static constexpr uint8_t CMD_RESUME_QUERY = 0x08u;

static constexpr uint8_t RESP_ACK = 0x06u;
static constexpr uint8_t RESP_NAK = 0x15u;

static constexpr uint8_t NAK_BAD_CRC   = 0x01u;
static constexpr uint8_t NAK_BAD_OFFS  = 0x02u;
static constexpr uint8_t NAK_FLASH_ERR = 0x03u;
static constexpr uint8_t NAK_BAD_STATE = 0x04u;

static std::vector<uint8_t> build_frame(uint8_t cmd, const std::vector<uint8_t>& payload)
{
    uint16_t             plen = static_cast<uint16_t>(payload.size());
    std::vector<uint8_t> frame;
    frame.push_back(SOF);
    frame.push_back(cmd);
    frame.push_back(static_cast<uint8_t>(plen & 0xFFu));
    frame.push_back(static_cast<uint8_t>(plen >> 8));
    frame.insert(frame.end(), payload.begin(), payload.end());
    std::vector<uint8_t> crc_input;
    crc_input.push_back(cmd);
    crc_input.push_back(static_cast<uint8_t>(plen & 0xFFu));
    crc_input.push_back(static_cast<uint8_t>(plen >> 8));
    crc_input.insert(crc_input.end(), payload.begin(), payload.end());
    uint32_t c = ymir::crc32_mpeg2({crc_input.data(), crc_input.size()});
    frame.push_back(static_cast<uint8_t>(c & 0xFFu));
    frame.push_back(static_cast<uint8_t>((c >> 8) & 0xFFu));
    frame.push_back(static_cast<uint8_t>((c >> 16) & 0xFFu));
    frame.push_back(static_cast<uint8_t>((c >> 24) & 0xFFu));
    return frame;
}

struct parsed_frame {
    uint8_t              cmd;
    std::vector<uint8_t> payload;
};

static std::vector<parsed_frame> parse_frames(const std::vector<uint8_t>& bytes)
{
    std::vector<parsed_frame> out;
    size_t                    i = 0;
    while (i + 8 <= bytes.size()) {
        assert(bytes[i] == SOF);
        uint8_t  cmd = bytes[i + 1];
        uint16_t len =
            static_cast<uint16_t>(bytes[i + 2]) | (static_cast<uint16_t>(bytes[i + 3]) << 8);
        size_t total = 4 + len + 4;
        assert(i + total <= bytes.size());
        parsed_frame f;
        f.cmd = cmd;
        f.payload.assign(bytes.begin() + i + 4, bytes.begin() + i + 4 + len);
        out.push_back(std::move(f));
        i += total;
    }
    assert(i == bytes.size());
    return out;
}

/* ── Fake transport ─────────────────────────────────────────────────────── */

struct ProtocolExit {};

class FakeTransport : public ymir::transport_t {
public:
    std::vector<uint8_t> rx_queue;
    size_t               rx_pos = 0;
    std::vector<uint8_t> tx_log;

    void push(const std::vector<uint8_t>& bytes)
    {
        rx_queue.insert(rx_queue.end(), bytes.begin(), bytes.end());
    }

    std::optional<uint8_t> rx_byte(uint32_t) override
    {
        if (rx_pos >= rx_queue.size()) {
            throw ProtocolExit{};
        }
        return rx_queue[rx_pos++];
    }

    bool tx_buf(std::span<const std::byte> buf) override
    {
        for (auto b : buf) {
            tx_log.push_back(std::to_integer<uint8_t>(b));
        }
        return true;
    }
};

static int run_protocol(FakeTransport& t)
{
    try {
        return protocol_run(t);
    } catch (ProtocolExit&) {
        return -99;
    }
}

/* ── Image building (matches scripts/prepare_image.py / test_image.cpp) ── */

static std::vector<uint8_t> build_image(uint8_t slot, size_t total_size)
{
    std::vector<uint8_t> img(total_size, 0xAA);
    auto*                h = reinterpret_cast<ymir::image::header_t*>(img.data());
    h->magic               = 0xB007AB1Eu;
    h->version             = 0x00010000u;
    h->image_size          = static_cast<uint32_t>(total_size);
    h->flags               = 0;
    h->slot                = slot;
    h->_reserved.fill(std::byte{0});
    ymir::sha256({reinterpret_cast<const std::byte*>(img.data() + 512), total_size - 512},
                 h->sha256);
    return img;
}

static void send_image(FakeTransport& t, uint8_t slot, const std::vector<uint8_t>& img)
{
    std::vector<uint8_t> start_payload = {slot};
    uint32_t             sz            = static_cast<uint32_t>(img.size());
    for (int i = 0; i < 4; i++) {
        start_payload.push_back(static_cast<uint8_t>(sz >> (i * 8)));
    }
    t.push(build_frame(CMD_START, start_payload));

    constexpr size_t CHUNK = 256;
    for (size_t off = 0; off < img.size(); off += CHUNK) {
        size_t               this_chunk = std::min(CHUNK, img.size() - off);
        std::vector<uint8_t> p;
        for (int i = 0; i < 4; i++) {
            p.push_back(static_cast<uint8_t>(off >> (i * 8)));
        }
        p.insert(p.end(), img.begin() + off, img.begin() + off + this_chunk);
        t.push(build_frame(CMD_CHUNK, p));
    }
    t.push(build_frame(CMD_FINISH, {}));
}

/* ── Tests ──────────────────────────────────────────────────────────────── */

static void seed_metadata(uint8_t slot, uint8_t boot_count, uint8_t confirmed)
{
    ymir::metadata::metadata_t m{};
    m.magic         = 0xBAADF00Du;
    m.generation    = 1;
    m.active_slot   = slot;
    m.boot_count    = boot_count;
    m.confirmed     = confirmed;
    m.transfer_slot = 0xFF;
    ymir::metadata::write(m);
}

static parsed_frame only_frame(FakeTransport& t)
{
    auto frames = parse_frames(t.tx_log);
    REQUIRE(frames.size() == 1);
    return frames[0];
}

TEST_CASE("protocol status returns metadata", "[protocol]")
{
    fake_flash_reset();
    seed_metadata(1, 2, 1);
    FakeTransport t;
    t.push(build_frame(CMD_STATUS, {}));
    run_protocol(t);

    auto f = only_frame(t);
    REQUIRE(f.cmd == CMD_STATUS);
    REQUIRE(f.payload.size() == sizeof(ymir::metadata::metadata_t));
    auto* m = reinterpret_cast<const ymir::metadata::metadata_t*>(f.payload.data());
    REQUIRE(m->active_slot == 1);
    REQUIRE(m->boot_count == 2);
    REQUIRE(m->confirmed == 1);
}

TEST_CASE("protocol start invalid slot", "[protocol]")
{
    fake_flash_reset();
    FakeTransport        t;
    std::vector<uint8_t> p = {2, 0, 0x10, 0, 0}; /* slot=2, size=0x1000 */
    t.push(build_frame(CMD_START, p));
    run_protocol(t);

    auto f = only_frame(t);
    REQUIRE(f.cmd == RESP_NAK);
    REQUIRE(f.payload.size() == 1);
    REQUIRE(f.payload[0] == NAK_BAD_STATE);
}

TEST_CASE("protocol start invalid size", "[protocol]")
{
    fake_flash_reset();
    FakeTransport        t;
    uint32_t             sz = 200u * 1024u; /* 200 KB > 192 KB cap */
    std::vector<uint8_t> p  = {0};
    for (int i = 0; i < 4; i++) {
        p.push_back(static_cast<uint8_t>(sz >> (i * 8)));
    }
    t.push(build_frame(CMD_START, p));
    run_protocol(t);

    auto f = only_frame(t);
    REQUIRE(f.cmd == RESP_NAK);
    REQUIRE(f.payload[0] == NAK_BAD_STATE);
}

TEST_CASE("protocol round trip", "[protocol]")
{
    fake_flash_reset();
    auto          img = build_image(0, 1024);
    FakeTransport t;
    send_image(t, 0, img);
    t.push(build_frame(CMD_BOOT, {0}));
    REQUIRE(run_protocol(t) == 0); /* booted slot 0 */

    auto frames = parse_frames(t.tx_log);
    /* START ack, 4 chunk acks (1024/256=4), FINISH ack, BOOT ack = 7 */
    REQUIRE(frames.size() == 7);
    for (auto& f : frames) {
        REQUIRE(f.cmd == RESP_ACK);
        REQUIRE(f.payload.empty());
    }
    REQUIRE(ymir::image::validate(reinterpret_cast<uintptr_t>(g_slot_a_flash)));
}

TEST_CASE("protocol chunk bad offset", "[protocol]")
{
    fake_flash_reset();
    FakeTransport        t;
    auto                 img           = build_image(0, 1024);
    std::vector<uint8_t> start_payload = {0};
    uint32_t             sz            = static_cast<uint32_t>(img.size());
    for (int i = 0; i < 4; i++) {
        start_payload.push_back(static_cast<uint8_t>(sz >> (i * 8)));
    }
    t.push(build_frame(CMD_START, start_payload));

    std::vector<uint8_t> p = {0x00, 0x04, 0x00, 0x00}; /* offset = 1024 */
    p.insert(p.end(), img.begin(), img.begin() + 256);
    t.push(build_frame(CMD_CHUNK, p));
    run_protocol(t);

    auto frames = parse_frames(t.tx_log);
    REQUIRE(frames.size() == 2);
    REQUIRE(frames[0].cmd == RESP_ACK);
    REQUIRE(frames[1].cmd == RESP_NAK);
    REQUIRE(frames[1].payload[0] == NAK_BAD_OFFS);
}

TEST_CASE("protocol finish invalid image", "[protocol]")
{
    fake_flash_reset();
    auto img = build_image(0, 1024);
    img[600] ^= 0xFF; /* corrupt one payload byte before transfer */
    FakeTransport t;
    send_image(t, 0, img);
    run_protocol(t);

    auto frames = parse_frames(t.tx_log);
    /* START ack, 4 chunk acks, FINISH nak */
    REQUIRE(frames.size() == 6);
    REQUIRE(frames[5].cmd == RESP_NAK);
    REQUIRE(frames[5].payload[0] == NAK_FLASH_ERR);
}

TEST_CASE("protocol abort resets state", "[protocol]")
{
    fake_flash_reset();
    auto                 img           = build_image(0, 1024);
    std::vector<uint8_t> start_payload = {0};
    uint32_t             sz            = static_cast<uint32_t>(img.size());
    for (int i = 0; i < 4; i++) {
        start_payload.push_back(static_cast<uint8_t>(sz >> (i * 8)));
    }
    FakeTransport t;
    t.push(build_frame(CMD_START, start_payload));
    t.push(build_frame(CMD_ABORT, {}));
    std::vector<uint8_t> p = {0, 0, 0, 0};
    p.insert(p.end(), img.begin(), img.begin() + 16);
    t.push(build_frame(CMD_CHUNK, p));
    run_protocol(t);

    auto frames = parse_frames(t.tx_log);
    REQUIRE(frames.size() == 3);
    REQUIRE(frames[0].cmd == RESP_ACK); /* START */
    REQUIRE(frames[1].cmd == RESP_ACK); /* ABORT */
    REQUIRE(frames[2].cmd == RESP_NAK); /* CHUNK after abort */
    REQUIRE(frames[2].payload[0] == NAK_BAD_STATE);
}

TEST_CASE("protocol resume query returns zero offset", "[protocol]")
{
    fake_flash_reset();
    FakeTransport t;
    t.push(build_frame(CMD_RESUME_QUERY, {0}));
    run_protocol(t);

    auto f = only_frame(t);
    REQUIRE(f.cmd == CMD_RESUME_QUERY);
    REQUIRE(f.payload.size() == 4);
    for (auto b : f.payload) {
        REQUIRE(b == 0);
    }
}

TEST_CASE("protocol bad crc responds nak", "[protocol]")
{
    fake_flash_reset();
    auto frame = build_frame(CMD_STATUS, {});
    frame.back() ^= 0xFF; /* corrupt the CRC */
    FakeTransport t;
    t.push(frame);
    run_protocol(t);

    auto f = only_frame(t);
    REQUIRE(f.cmd == RESP_NAK);
    REQUIRE(f.payload[0] == NAK_BAD_CRC);
}

TEST_CASE("protocol unknown command responds nak", "[protocol]")
{
    fake_flash_reset();
    FakeTransport t;
    t.push(build_frame(0xFE, {}));
    run_protocol(t);

    auto f = only_frame(t);
    REQUIRE(f.cmd == RESP_NAK);
    REQUIRE(f.payload[0] == NAK_BAD_STATE);
}
