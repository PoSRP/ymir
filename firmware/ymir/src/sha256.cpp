#include "ymir/sha256.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>

namespace ymir {

// TODO: Rename to something understandable
static constexpr std::array<uint32_t, 64> K = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2,
};

static uint32_t rotr32(uint32_t x, unsigned n) { return (x >> n) | (x << (32u - n)); }

struct sha256_ctx_t {
    std::array<uint32_t, 8> state;
    std::array<uint8_t, 64> buf;
    uint64_t                total;  // total bytes processed
    uint32_t                buflen; // bytes currently in buf
};

static void compress(uint32_t* s, const uint8_t* blk)
{
    std::array<uint32_t, 64> w;
    for (int i = 0; i < 16; i++) {
        w[i] = ((uint32_t)blk[i * 4 + 0] << 24) | ((uint32_t)blk[i * 4 + 1] << 16) |
               ((uint32_t)blk[i * 4 + 2] << 8) | ((uint32_t)blk[i * 4 + 3]);
    }
    for (int i = 16; i < 64; i++) {
        uint32_t s0 = rotr32(w[i - 15], 7) ^ rotr32(w[i - 15], 18) ^ (w[i - 15] >> 3);
        uint32_t s1 = rotr32(w[i - 2], 17) ^ rotr32(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i]        = w[i - 16] + s0 + w[i - 7] + s1;
    }

    uint32_t a = s[0], b = s[1], c = s[2], d = s[3];
    uint32_t e = s[4], f = s[5], g = s[6], h = s[7];

    for (int i = 0; i < 64; i++) {
        uint32_t S1  = rotr32(e, 6) ^ rotr32(e, 11) ^ rotr32(e, 25);
        uint32_t ch  = (e & f) ^ (~e & g);
        uint32_t t1  = h + S1 + ch + K[i] + w[i];
        uint32_t S0  = rotr32(a, 2) ^ rotr32(a, 13) ^ rotr32(a, 22);
        uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        uint32_t t2  = S0 + maj;
        h            = g;
        g            = f;
        f            = e;
        e            = d + t1;
        d            = c;
        c            = b;
        b            = a;
        a            = t1 + t2;
    }

    s[0] += a;
    s[1] += b;
    s[2] += c;
    s[3] += d;
    s[4] += e;
    s[5] += f;
    s[6] += g;
    s[7] += h;
}

static void ctx_init(sha256_ctx_t& ctx)
{
    ctx.state[0] = 0x6a09e667u;
    ctx.state[1] = 0xbb67ae85u;
    ctx.state[2] = 0x3c6ef372u;
    ctx.state[3] = 0xa54ff53au;
    ctx.state[4] = 0x510e527fu;
    ctx.state[5] = 0x9b05688cu;
    ctx.state[6] = 0x1f83d9abu;
    ctx.state[7] = 0x5be0cd19u;
    ctx.total    = 0;
    ctx.buflen   = 0;
}

static void ctx_update(sha256_ctx_t& ctx, const uint8_t* data, size_t len)
{
    ctx.total += len;
    while (len > 0) {
        uint32_t room = 64 - ctx.buflen;
        uint32_t take = (len < room) ? (uint32_t)len : room;
        std::memcpy(ctx.buf.data() + ctx.buflen, data, take);
        ctx.buflen += take;
        data += take;
        len -= take;
        if (ctx.buflen == 64) {
            compress(ctx.state.data(), ctx.buf.data());
            ctx.buflen = 0;
        }
    }
}

static void ctx_final(sha256_ctx_t& ctx, uint8_t* digest)
{
    uint64_t bits = ctx.total * 8u;

    // Append 0x80 padding byte
    // There is always room since buflen < 64
    ctx.buf[ctx.buflen++] = 0x80;

    // If the padding byte plus length field (8 bytes) don't fit in the
    //   current block, flush the current block first
    if (ctx.buflen > 56) {
        std::memset(ctx.buf.data() + ctx.buflen, 0, 64 - ctx.buflen);
        compress(ctx.state.data(), ctx.buf.data());
        ctx.buflen = 0;
    }

    // Zero-pad to byte 56, then append the 64-bit big-endian bit count
    std::memset(ctx.buf.data() + ctx.buflen, 0, 56 - ctx.buflen);
    for (int i = 0; i < 8; i++) {
        ctx.buf[63 - i] = (uint8_t)(bits >> (i * 8));
    }
    compress(ctx.state.data(), ctx.buf.data());

    for (int i = 0; i < 8; i++) {
        digest[i * 4 + 0] = (uint8_t)(ctx.state[i] >> 24);
        digest[i * 4 + 1] = (uint8_t)(ctx.state[i] >> 16);
        digest[i * 4 + 2] = (uint8_t)(ctx.state[i] >> 8);
        digest[i * 4 + 3] = (uint8_t)(ctx.state[i]);
    }
}

void sha256(std::span<const std::byte> data, std::array<std::byte, 32>& digest)
{
    sha256_ctx_t ctx;
    ctx_init(ctx);
    if (!data.empty()) {
        ctx_update(ctx, reinterpret_cast<const uint8_t*>(data.data()), data.size());
    }
    ctx_final(ctx, reinterpret_cast<uint8_t*>(digest.data()));
}

} // namespace ymir
