#include "ymir/image.hpp"
#include "ymir/sha256.hpp"

#include <array>
#include <cstring>

#include <catch2/catch_test_macros.hpp>

alignas(4) static std::array<std::byte, 1024> image_buf;

static void make_valid_image()
{
    image_buf.fill(std::byte{0xAA});
    auto* h       = reinterpret_cast<ymir::image::header_t*>(image_buf.data());
    h->magic      = 0xB007AB1Eu;
    h->version    = 0x00010000u;
    h->image_size = 1024;
    h->flags      = 0;
    h->slot       = 0;
    h->_reserved.fill(std::byte{0});
    ymir::sha256(std::span<const std::byte>{image_buf.data() + 512, 512u}, h->sha256);
}

TEST_CASE("image valid", "[image]")
{
    make_valid_image();
    REQUIRE(ymir::image::validate(reinterpret_cast<uintptr_t>(image_buf.data())));
}

TEST_CASE("image wrong magic", "[image]")
{
    make_valid_image();
    reinterpret_cast<ymir::image::header_t*>(image_buf.data())->magic = 0xDEADBEEFu;
    REQUIRE(!ymir::image::validate(reinterpret_cast<uintptr_t>(image_buf.data())));
}

TEST_CASE("image nonzero flags", "[image]")
{
    make_valid_image();
    reinterpret_cast<ymir::image::header_t*>(image_buf.data())->flags = 1;
    REQUIRE(!ymir::image::validate(reinterpret_cast<uintptr_t>(image_buf.data())));
}

TEST_CASE("image payload bitflip", "[image]")
{
    make_valid_image();
    image_buf[600] ^= std::byte{0xFF};
    REQUIRE(!ymir::image::validate(reinterpret_cast<uintptr_t>(image_buf.data())));
}

TEST_CASE("image size too small", "[image]")
{
    make_valid_image();
    reinterpret_cast<ymir::image::header_t*>(image_buf.data())->image_size = 512;
    REQUIRE(!ymir::image::validate(reinterpret_cast<uintptr_t>(image_buf.data())));
}

TEST_CASE("image size too large", "[image]")
{
    make_valid_image();
    reinterpret_cast<ymir::image::header_t*>(image_buf.data())->image_size = 193u * 1024u;
    REQUIRE(!ymir::image::validate(reinterpret_cast<uintptr_t>(image_buf.data())));
}
