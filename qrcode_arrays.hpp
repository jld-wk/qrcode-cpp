#ifndef QRCODE_ARRAYS_HPP
#define QRCODE_ARRAYS_HPP

#include <array>
#include <cstddef>
#include <cstdint>

template <typename ElementType, size_t Size>
class StackArray {
 public:
  using value_type = ElementType;
  using size_type = std::size_t;
  using iterator = value_type*;
  using const_iterator = const value_type*;

  std::array<ElementType, Size> data{};
  size_t                        size = 0;

  StackArray() = default;
  ~StackArray() = default;
  StackArray(const StackArray&) = delete;
  StackArray(StackArray&&) = delete;
  StackArray& operator=(const StackArray&) = delete;
  StackArray& operator=(StackArray&&) = delete;

  [[__nodiscard__]] constexpr iterator begin() noexcept {
    return data.data();
  }

  [[__nodiscard__]] constexpr const_iterator begin() const noexcept {
    return data.data();
  }

  [[__nodiscard__]] constexpr iterator end() noexcept {
    return data.data() + size;
  }

  [[__nodiscard__]] constexpr const_iterator end() const noexcept {
    return data.data() + size;
  }

  [[__nodiscard__]] constexpr value_type& operator[](size_type i) noexcept {
    return data[i];
  }

  [[__nodiscard__]] constexpr const value_type& operator[](size_type i) const noexcept {
    return data[i];
  }
};

// TODO: 67 is wrong because we need to take ec codewords into account as well
using CodewordBlock = StackArray<uint8_t, 70>;
// TODO: is 123 correct? max for group 1 is actually: 49, 122
using CodewordArray = StackArray<uint8_t, 70 * 200>;

// 18 elements is the largest operation the qr code generator needs. Is that true, future me?
using BchArray = StackArray<uint8_t, 18>;

#endif