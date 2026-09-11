// SPDX-FileCopyrightText: 2026 Julian Duwe
// SPDX-License-Identifier: Apache-2.0

#ifndef JLD_QRCODE_COMMON
#define JLD_QRCODE_COMMON

#include <array>
#include <cstddef>
#include <cstdint>

// This file is shared with the memoizer tool and the qrcode generation implementation

namespace jld {

// c_maxPolyCoeffsSize   = Largest 'EC/block + 1'
// c_maxPolyGenSize      = Largest 'EC/block'
// c_maxPolyDivDataSize  = Largest 'EC/block + G2 data/block'
// c_maxCodewordsCount   = Largest 'Total Codewords'
// c_maxModulesCount     = Largest 'Total Modules'

static inline constexpr size_t c_maxPolyCoeffsSize = 31;
static inline constexpr size_t c_maxPolyDivDataSize = 153;

static inline constexpr uint8_t c_maxPolyGenSize = 30;

// Can be edited to reduce the amount of pre-allocated modules, if not needed
static inline constexpr size_t c_maxModulesCount = 31329;
// Can be edited to reduce the amount of pre-allocated codewords, if not needed
static inline constexpr size_t c_maxCodewordsCount = 3706;

// Wrapper of std::array<uint8_t, Size> with an extra variable to track the actually set data
template <size_t Size>
class ByteArray {
 public:
  size_t                    size{ 0 };
  std::array<uint8_t, Size> data{};

  static constexpr size_t c_size_ = Size;

  ByteArray() = default;
  ~ByteArray() = default;
  ByteArray(const ByteArray&) = delete;
  ByteArray(ByteArray&&) = delete;
  auto operator=(const ByteArray&) -> ByteArray& = delete;
  auto operator=(ByteArray&&) -> ByteArray& = delete;

  [[nodiscard]] constexpr auto begin() noexcept -> std::array<uint8_t, Size>::iterator {
    return data.data();
  }

  [[nodiscard]] constexpr auto begin() const noexcept -> std::array<uint8_t, Size>::const_iterator {
    return data.data();
  }

  [[nodiscard]] constexpr auto end() noexcept -> std::array<uint8_t, Size>::iterator {
    return data.data() + size;
  }

  [[nodiscard]] constexpr auto end() const noexcept -> std::array<uint8_t, Size>::const_iterator {
    return data.data() + size;
  }

  [[nodiscard]] constexpr auto operator[](size_t i) noexcept -> uint8_t& {
    return data[i];
  }

  [[nodiscard]] constexpr auto operator[](size_t i) const noexcept -> const uint8_t& {
    return data[i];
  }
};

using BchArray = ByteArray<18>;

}  // namespace jld

#endif  // JLD_QRCODE_COMMON