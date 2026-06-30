#ifndef YMIR_SHA256_HPP
#define YMIR_SHA256_HPP

#include <array>
#include <cstddef>
#include <span>

namespace ymir {

void sha256(std::span<const std::byte> data, std::array<std::byte, 32>& digest);

} // namespace ymir

#endif // YMIR_SHA256_HPP
