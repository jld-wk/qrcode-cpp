#ifndef QRCODE_HPP
#define QRCODE_HPP

#include <algorithm>
#include <array>
#include <bitset>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iostream>
#include <limits>
#include <ranges>
#include <vector>

#include "qrcode_pixels.hpp"

enum class Ecc : uint8_t { M = 0, L = 1, H = 2, Q = 3 };

struct CodewordInfo {
  uint16_t total;
  uint16_t error;
  uint16_t data;
};

constexpr std::array<std::array<CodewordInfo, 4>, 41> c_qrCodewords = { {
    {},
    { {
        { 26, 10, 16 },  // M
        { 26, 7, 19 },   // L
        { 26, 17, 9 },   // H
        { 26, 13, 13 },  // Q
    } },
    { {
        { 44, 16, 28 },  // M
        { 44, 10, 34 },  // L
        { 44, 28, 16 },  // H
        { 44, 22, 22 },  // Q
    } },
    { {
        { 70, 26, 44 },  // M
        { 70, 15, 55 },  // L
        { 70, 44, 26 },  // H
        { 70, 36, 34 },  // Q
    } },
    { {
        { 100, 36, 64 },  // M
        { 100, 20, 80 },  // L
        { 100, 64, 36 },  // H
        { 100, 52, 48 },  // Q
    } },
    // TODO: ...
} };

class CodewordBits {
 public:
  CodewordBits(size_t bit_len, const std::vector<uint8_t>& codewords)
      : m_bitLen_{ bit_len }
      , m_bitIdx_{ bit_len - 1 }
      , m_codewords_{ codewords }
      , m_codewordBits_{ codewords[0] } {}

  bool get() {
    size_t  idx = m_bitIdx_--;
    uint8_t bit = m_codewordBits_[idx];

    if (idx == 0) {
      ++m_wordIdx_;
      m_bitIdx_ = m_bitLen_ - 1;
      if (m_wordIdx_ < m_codewords_.size())
        m_codewordBits_ = m_codewords_[m_wordIdx_];
    }
    return bit == 1;
  }

  bool at_last_bit() const {
    return m_bitIdx_ == 7;
  }

  bool in_bounds() const {
    return m_wordIdx_ != m_codewords_.size();
  }

 private:
  size_t m_bitLen_ = 0;
  size_t m_bitIdx_ = 0;
  size_t m_wordIdx_ = 0;

  const std::vector<uint8_t>& m_codewords_;
  std::bitset<8>              m_codewordBits_;
};

class QrCode {
 public:
  QrCode(const std::vector<uint8_t>& data, size_t version, Ecc ecc, bool debug = false)
      : m_qrWh_{ qrcode_wh(version) }
      , m_pixels_{ m_qrWh_ }
      , m_maskedPixels_(m_qrWh_ * m_qrWh_, 0) {
    CodewordInfo info = c_qrCodewords[version][static_cast<size_t>(ecc)];

    if (data.size() > (info.data - 2)) {
      std::cerr << "Too many bytes of data to encode!\n";
      m_pixels_.write_jpg();  // TODO: Remove
      return;
    }

    std::vector<uint8_t> codewords(info.total);
    fill_codewords(codewords, info.data, data);

    polynomial_generator(info.error);
    polynomial_division(codewords, info.data);

    if (debug) {
      std::cout << "Coeffs: [ ";
      for (size_t i = 0; i < m_polyCoeffsSize_; ++i) std::cout << m_polyCoeffs_[i] + 0 << " - ";
      std::cout << " ]";

      std::cout << "\n\n";

      std::cout << "Codewords (Binary): [ ";
      for (uint8_t c : codewords) std::cout << std::bitset<8>(c) << " - ";
      std::cout << " ]";

      std::cout << "\n\n";

      std::cout << "Codewords (Decimal): [ ";
      for (uint8_t c : codewords) std::cout << c + 0 << " - ";
      std::cout << " ]";

      std::cout << "\n\n";
    }

    uint8_t bit_len = codeword_bits(version);

    set_reserved_pixels(version);
    if (!debug)
      set_data_pixels(bit_len, codewords);

    size_t mask_idx = debug ? 0 : find_best_mask();
    set_format_pixels(mask_idx, ecc, debug);

    if (!debug)
      mask_data_pixels(mask_idx);

    if (!debug) {
      for (size_t i = 0; i < m_maskedPixels_.size(); ++i)
        m_pixels_.pixels()[i] = (m_maskedPixels_[i] == 1 ? 0 : 255);
    }

    m_pixels_.write_jpg();
  }

 private:
  size_t qrcode_wh(size_t version) const {
    return version * 4 + 17;
  }

  uint8_t codeword_bits(uint8_t version) const {
    return version >= 10 ? 16 : 8;
  }

  void fill_codewords(std::vector<uint8_t>& codewords, uint8_t to_fill,
                      const std::vector<uint8_t>& data) const {
    size_t data_size = data.size();
    if (data.size() > to_fill)
      return;

    uint8_t begin{ 0b01000000 };
    begin |= data_size >> 4;
    codewords[0] = begin;
    codewords[1] = data_size << 4;

    size_t idx{ 1 };
    for (uint8_t b : data) {
      codewords[idx++] |= b >> 4;
      codewords[idx] = b << 4;
    }

    codewords[idx++] &= 0b11110000;

    bool alternate{ false };
    while (idx != to_fill) {
      codewords[idx++] = alternate ? 0b00010001 : 0b11101100;
      alternate = !alternate;
    }
  }

  void set_data_pixels(uint8_t bit_len, const std::vector<uint8_t> codewords) {
    bool   up_dir{ true };
    size_t cur_row{ m_qrWh_ - 1 };
    // TODO: Address?
    int32_t cur_col{ static_cast<int32_t>(m_qrWh_ - 1) };

    CodewordBits bits{ bit_len, codewords };
    m_pixels_.fill_pixel(cur_row, cur_col, bits.get());

    while (bits.in_bounds()) {
      m_pixels_.fill_pixel(cur_row, --cur_col, bits.get());

      if (!bits.in_bounds())
        break;

      bool is_last_row = (up_dir && cur_row <= 0) || (!up_dir && cur_row >= m_qrWh_ - 1);

      if (!is_last_row) {
        size_t next_row{ up_dir ? cur_row - 1 : cur_row + 1 };
        while (!m_pixels_.is_reserved(next_row, cur_col) &&
               m_pixels_.is_reserved(next_row, cur_col + 1)) {
          up_dir ? --cur_row : ++cur_row;
          next_row = up_dir ? cur_row - 1 : cur_row + 1;
          m_pixels_.fill_pixel(cur_row, cur_col, bits.get());
        }

        is_last_row = true;
        if (up_dir) {
          for (size_t r = cur_row; r-- != 0;) {
            if (!m_pixels_.is_reserved(r, cur_col + 1)) {
              cur_row = r;
              is_last_row = false;
              break;
            }
          }
        } else {
          for (size_t r = cur_row + 1; r < m_qrWh_; ++r) {
            if (!m_pixels_.is_reserved(r, cur_col + 1)) {
              cur_row = r;
              is_last_row = false;
              break;
            }
          }
        }
      }

      if (!is_last_row) {
        m_pixels_.fill_pixel(cur_row, ++cur_col, bits.get());
        continue;
      }

      if (!bits.in_bounds())
        break;

      if (m_pixels_.is_reserved(cur_row, --cur_col)) {
        if (!m_pixels_.is_reserved(cur_row, cur_col - 1))
          --cur_col;
        else {
          if (up_dir) {
            for (size_t r = cur_row + 1; r < m_qrWh_; ++r) {
              if (!m_pixels_.is_reserved(r, cur_col)) {
                cur_row = r;
                break;
              }
            }
          } else {
            for (size_t r = cur_row; r-- != 0;) {
              if (!m_pixels_.is_reserved(r, cur_col)) {
                cur_row = r;
                break;
              }
            }
          }
        }
      }

      m_pixels_.fill_pixel(cur_row, cur_col, bits.get());
      up_dir = !up_dir;
    }
  }

  void set_reserved_pixels(uint8_t version) {
    // Reserving finder patterns
    m_pixels_.fill_area(0, 0, 9, 9, true, true);
    m_pixels_.fill_area(0, m_qrWh_ - 8, 8, 9, true, true);
    m_pixels_.fill_area(m_qrWh_ - 8, 0, 9, 8, true, true);

    // Reserving alignment patterns
    m_pixels_.fill_area(m_qrWh_ - 9, m_qrWh_ - 9, 5, 5, true, true);

    // Reserving timing patterns
    m_pixels_.fill_area(6, 9, version * 4, 1, true, true);
    m_pixels_.fill_area(9, 6, 1, version * 4, true, true);

    // Reserving dark module
    m_pixels_.fill_pixel(m_qrWh_ - 8, 8, true, true);

    struct ReservedArea {
      size_t row{ 0 };
      size_t col{ 0 };
      size_t width{ 0 };
      size_t height{ 0 };
      size_t finder_width{ 0 };
      size_t finder_height{ 0 };
    };

    const std::vector<ReservedArea> reserved_areas{
      ReservedArea{
          .row = 0, .col = 0, .width = 7, .height = 7, .finder_width = 9, .finder_height = 9 },
      ReservedArea{ .row = 0,
                    .col = m_qrWh_ - 7,
                    .width = 7,
                    .height = 7,
                    .finder_width = 8,
                    .finder_height = 9 },
      ReservedArea{ .row = m_qrWh_ - 7,
                    .col = 0,
                    .width = 7,
                    .height = 7,
                    .finder_width = 9,
                    .finder_height = 8 },
      ReservedArea{ .row = m_qrWh_ - 9, .col = m_qrWh_ - 9, .width = 5, .height = 5 },
    };

    for (const ReservedArea& area : reserved_areas) {
      m_pixels_.fill_area(area.row + 1, area.col + 1, 1, area.height - 2, false, true);
      m_pixels_.fill_area(area.row + 1, area.col + (area.height - 2), 1, area.height - 2, false,
                          true);
      m_pixels_.fill_area(area.row + 1, area.col + 1, area.width - 2, 1, false, true);
      m_pixels_.fill_area(area.row + (area.width - 2), area.col + 1, area.width - 2, 1, false,
                          true);

      if (area.finder_width > area.width) {
        size_t  diff{ area.finder_width - area.width };
        size_t  height_diff{ area.finder_height - area.height };
        int32_t orientation{ static_cast<int32_t>(area.col == 0 ? area.width : -diff) };

        m_pixels_.fill_area(area.row - (area.row == 0 ? 0 : height_diff), area.col + orientation,
                            diff, area.height + height_diff, false, true);
      }

      if (area.finder_height > area.height) {
        size_t  diff{ area.finder_height - area.height };
        size_t  width_diff{ area.finder_width - area.width };
        int32_t orientation{ static_cast<int32_t>(area.row == 0 ? area.height : -diff) };

        m_pixels_.fill_area(area.row + orientation, area.col - (area.col == 0 ? 0 : width_diff),
                            area.width + width_diff, diff, false, true);
      }
    }

    m_pixels_.fill_area(6, 9, version * 4, 1, false, true);
    m_pixels_.fill_area(9, 6, 1, version * 4, false, true);

    for (size_t i = 8; i < 10 + version * 4; i += 2) m_pixels_.fill_pixel(6, i, true, true);
    for (size_t i = 8; i < 10 + version * 4; i += 2) m_pixels_.fill_pixel(i, 6, true, true);

    m_pixels_.fill_pixel(m_qrWh_ - 8, 8, true, true);
  }

  void set_format_pixels(uint8_t mask_idx, Ecc ecc, bool debug = false) {
    std::vector<uint8_t> format_poly(15);

    const uint8_t ecc_idx{ static_cast<uint8_t>(ecc) };

    format_poly[0] = ecc_idx >> 1;
    format_poly[1] = ecc_idx & 1;
    format_poly[2] = mask_idx >> 2;
    format_poly[3] = (mask_idx >> 1) & 1;
    format_poly[4] = mask_idx & 1;

    m_polyCoeffs_[0] = 1;
    m_polyCoeffs_[1] = 0;
    m_polyCoeffs_[2] = 1;
    m_polyCoeffs_[3] = 0;
    m_polyCoeffs_[4] = 0;
    m_polyCoeffs_[5] = 1;
    m_polyCoeffs_[6] = 1;
    m_polyCoeffs_[7] = 0;
    m_polyCoeffs_[8] = 1;
    m_polyCoeffs_[9] = 1;
    m_polyCoeffs_[10] = 1;

    m_polyCoeffsSize_ = 11;

    static std::vector<uint8_t> debug_data{ 0,   18,  36,  54,  73,  91,  109, 128,
                                            146, 164, 182, 201, 219, 237, 255 };

    if (debug) {
      std::cout << "Format Poly (Pre): [ ";
      for (size_t i = 0; i < format_poly.size(); ++i) std::cout << format_poly[i] + 0 << " - ";
      std::cout << " ]\n\n";
    }

    polynomial_division(format_poly, 5, true);

    if (debug) {
      std::cout << "Format Poly (Post-Division): [ ";
      for (size_t i = 0; i < format_poly.size(); ++i) std::cout << format_poly[i] + 0 << " - ";
      std::cout << " ]\n\n";
    }

    static constexpr std::array<uint8_t, 15> format_mask{ 1, 0, 1, 0, 1, 0, 0, 0,
                                                          0, 0, 1, 0, 0, 1, 0 };

    for (size_t i = 0; i < format_poly.size(); ++i) format_poly[i] ^= format_mask[i];

    if (debug) {
      std::cout << "Format Poly (Post-Mask): [ ";
      for (size_t i = 0; i < format_poly.size(); ++i) std::cout << format_poly[i] + 0 << " - ";
      std::cout << " ]\n\n";
    }

    // fill_area with custom data inverts the rows min/max!!

    m_pixels_.fill_area(m_qrWh_ - 9, 0, 6, 1, debug ? debug_data : format_poly, 0, debug);
    m_pixels_.fill_area(m_qrWh_ - 9, 7, 1, 1, debug ? debug_data : format_poly, 6, debug);
    m_pixels_.fill_area(m_qrWh_ - 9, 8, 1, 2, debug ? debug_data : format_poly, 7, debug);
    m_pixels_.fill_area(m_qrWh_ - 6, 8, 1, 6, debug ? debug_data : format_poly, 9, debug);

    m_pixels_.fill_area(0, 8, 1, 7, debug ? debug_data : format_poly, 0, debug);
    m_pixels_.fill_area(m_qrWh_ - 9, m_qrWh_ - 8, 8, 1, debug ? debug_data : format_poly, 7, debug);
  }

  uint8_t find_best_mask() {
    uint8_t best_mask = 0;
    size_t  best_penalty = SIZE_MAX;

    for (uint8_t i = 0; i < 8; ++i) {
      mask_data_pixels(i);
      size_t penalty = compute_mask_penalty();
      if (penalty < best_penalty) {
        best_mask = i;
        best_penalty = penalty;
      }
    }

    return best_mask;
  }

  void mask_data_pixels(uint8_t mask_idx) {
    static constexpr std::array<uint8_t (*)(size_t, size_t), 8> mask_functions = {
      [](size_t row, size_t column) -> uint8_t { return ((row + column) % 2) == 0; },
      [](size_t row, size_t column) -> uint8_t { return (row % 2) == 0; },
      [](size_t row, size_t column) -> uint8_t { return (column % 3) == 0; },
      [](size_t row, size_t column) -> uint8_t { return ((row + column) % 3) == 0; },
      [](size_t row, size_t column) -> uint8_t {
        return ((static_cast<size_t>(std::floor(row / 2)) +
                 static_cast<size_t>(std::floor(column / 3))) %
                2) == 0;
      },
      [](size_t row, size_t column) -> uint8_t {
        return (row * column % 2 + row * column % 3) == 0;
      },
      [](size_t row, size_t column) -> uint8_t {
        return (((row * column) % 2 + row * column % 3) % 2) == 0;
      },
      [](size_t row, size_t column) -> uint8_t {
        return (((row + column) % 2 + row * column % 3) % 2) == 0;
      }
    };

    for (size_t r = 0; r < m_qrWh_; ++r) {
      for (size_t c = 0; c < m_qrWh_; ++c) {
        // TODO: use this directly on the vectors, instead of is_reserved, pixel, etc.
        size_t pixel_idx{ m_pixels_.pixel_idx(r, c) };

        uint8_t value{ static_cast<uint8_t>((m_pixels_.pixel(r, c) == 0 ? 1 : 0)) };
        bool    is_reserved = m_pixels_.is_reserved(r, c);
        if (is_reserved) {
          m_maskedPixels_[pixel_idx] = value;
          continue;
        }

        // TODO: Mask function
        m_maskedPixels_[pixel_idx] = value ^ mask_functions[mask_idx](r, c);
      }
    }
  }

  size_t compute_mask_penalty() {
    size_t penalty{ 0 };

    size_t  rule1_count{ 0 };
    uint8_t rule1_counting{ 0 };

    auto calc_rule1 = [&](uint8_t value) {
      if (value != rule1_counting) {
        rule1_counting = value;
        rule1_count = 1;
        return;
      }

      ++rule1_count;
      if (rule1_count == 5)
        penalty += 3;
      else if (rule1_count > 5)
        ++penalty;
    };

    auto calc_rule2 = [&](size_t row, size_t column, uint8_t value) {
      if ((column + 1 >= m_qrWh_) || (row + 1 >= m_qrWh_))
        return;

      size_t tr{ m_pixels_.pixel_idx(row, column + 1) };
      size_t bl{ m_pixels_.pixel_idx(row + 1, column) };
      size_t br{ m_pixels_.pixel_idx(row + 1, column + 1) };

      if (m_maskedPixels_[tr] == value && m_maskedPixels_[bl] == value &&
          m_maskedPixels_[br] == value)
        penalty += 3;
    };

    size_t rule4_dark_modules{ 0 };
    for (size_t r = 0; r < m_qrWh_; ++r) {
      for (size_t c = 0; c < m_qrWh_; ++c) {
        size_t  pixel_idx{ m_pixels_.pixel_idx(r, c) };
        uint8_t value = m_maskedPixels_[pixel_idx];
        if (value == 1)
          ++rule4_dark_modules;
        calc_rule1(value);
        calc_rule2(r, c, value);
        calc_rule3<true>(r, c, value, &penalty);
      }
    }

    for (size_t c = 0; c < m_qrWh_; ++c) {
      for (size_t r = 0; r < m_qrWh_; ++r) {
        size_t  pixel_idx{ m_pixels_.pixel_idx(r, c) };
        uint8_t value = m_maskedPixels_[pixel_idx];
        calc_rule1(value);
        calc_rule3<false>(c, r, value, &penalty);
      }
    }

    double rule4_total_modules{ static_cast<double>(m_qrWh_ * m_qrWh_) };
    size_t rule4_dark_percentage{ static_cast<size_t>(
        std::floor((rule4_dark_modules / rule4_total_modules) * 100)) };

    rule4_dark_percentage = 67;

    if (rule4_dark_percentage > 50)
      rule4_dark_percentage -= (rule4_dark_percentage % 5);
    else
      rule4_dark_percentage =
          (rule4_dark_percentage + 5 - 1) - ((rule4_dark_percentage + 5 - 1) % 5);

    int32_t rule4_diff{ static_cast<int32_t>(50 - rule4_dark_percentage) };
    return penalty + std::abs(rule4_diff) * 2;
  }

  template <bool BIsColumn>
  void calc_rule3(size_t a_, size_t b_, uint8_t value, size_t* penalty) {
    static constexpr std::array<uint8_t, 7> rule3_pattern = { 1, 0, 1, 1, 1, 0, 1 };

    size_t r = BIsColumn ? a_ : b_;
    size_t c = BIsColumn ? b_ : a_;

    if (BIsColumn ? (c > m_qrWh_ - 11) : (r >= 11))
      return;

    auto row = [&](size_t to_add) { return BIsColumn ? r : r + to_add; };
    auto column = [&](size_t to_add) { return BIsColumn ? c + to_add : c; };

    uint8_t module_0 = value;
    uint8_t module_1 = m_maskedPixels_[m_pixels_.pixel_idx(row(1), column(1))];
    uint8_t module_2 = m_maskedPixels_[m_pixels_.pixel_idx(row(2), column(2))];
    uint8_t module_3 = m_maskedPixels_[m_pixels_.pixel_idx(row(3), column(3))];

    bool whites_at_begin = false;
    if (module_0 == 0 && module_1 == 0 && module_2 == 0 && module_3 == 0)
      whites_at_begin = true;

    if (whites_at_begin) {
      for (size_t i = 0; i < rule3_pattern.size(); ++i) {
        if (rule3_pattern[i] != m_maskedPixels_[m_pixels_.pixel_idx(row(4 + i), column(4 + i))])
          return;
      }
      *penalty += 40;
      return;
    }

    if (rule3_pattern[0] != module_0 || rule3_pattern[1] != module_1 ||
        rule3_pattern[2] != module_2 || rule3_pattern[3] != module_3)
      return;

    uint8_t module_4 = m_maskedPixels_[m_pixels_.pixel_idx(row(4), column(4))];
    uint8_t module_5 = m_maskedPixels_[m_pixels_.pixel_idx(row(5), column(5))];
    uint8_t module_6 = m_maskedPixels_[m_pixels_.pixel_idx(row(6), column(6))];

    if (rule3_pattern[4] != module_4 || rule3_pattern[5] != module_5 ||
        rule3_pattern[6] != module_6)
      return;

    uint8_t module_7 = m_maskedPixels_[m_pixels_.pixel_idx(row(7), column(7))];
    uint8_t module_8 = m_maskedPixels_[m_pixels_.pixel_idx(row(8), column(8))];
    uint8_t module_9 = m_maskedPixels_[m_pixels_.pixel_idx(row(9), column(9))];
    uint8_t module_10 = m_maskedPixels_[m_pixels_.pixel_idx(row(10), column(10))];

    if (module_7 == 0 && module_8 == 0 && module_9 == 0 && module_10 == 0)
      *penalty += 40;
  }

  uint8_t gf256_mul(uint8_t a, uint8_t b) const {
    return m_expTable_[(m_logTable_[a] + m_logTable_[b]) % 255];
  }

  void polynomial_multiplication(uint8_t factor) {
    uint8_t prev_element = m_polyCoeffs_[0];

    for (size_t i = 1; i < m_polyCoeffsSize_; ++i) {
      uint8_t cur = m_polyCoeffs_[i];

      if (cur == 0) {
        m_polyCoeffs_[i] = cur;
        continue;
      }

      uint8_t matched = gf256_mul(prev_element, factor);
      prev_element = cur;
      m_polyCoeffs_[i] = cur ^ matched;
    }

    m_polyCoeffs_[m_polyCoeffsSize_] = gf256_mul(prev_element, factor);
    ++m_polyCoeffsSize_;
  }

  void polynomial_generator(uint8_t degree) {
    m_polyCoeffs_[0] = 1;
    m_polyCoeffsSize_ = 1;

    for (uint8_t i = 0; i < degree; ++i) polynomial_multiplication(m_expTable_[i]);
  }

  void polynomial_division(std::vector<uint8_t>& data, uint8_t actual_data_size, bool bch = false) {
    std::vector<uint8_t> ecc{ data };
    size_t               ecc_size{ ecc.size() };

    for (size_t i = 0; i < actual_data_size; ++i) {
      uint8_t start{ ecc[i] };
      if (start != 0) {
        for (size_t g = 0; g < m_polyCoeffsSize_; ++g)
          ecc[i + g] ^= (bch ? m_polyCoeffs_[g] : gf256_mul(start, m_polyCoeffs_[g]));
      }
    }

    for (size_t i = actual_data_size; i < ecc_size; ++i) data[i] = ecc[i];
  }

  constexpr std::array<uint8_t, 256> make_exp_table() const {
    std::array<uint8_t, 256> exp_table{ 1 };
    uint8_t                  prev_res{ 1 };

    for (size_t i = 1; i < exp_table.size(); ++i) {
      if (prev_res > 127) {
        uint8_t res = prev_res;
        res <<= 1;
        if (prev_res & 0b10000000)
          res ^= 0b00011101;
        prev_res = res;
        exp_table[i] = res;
        continue;
      }

      uint8_t res = prev_res * 2;
      exp_table[i] = res;
      prev_res = res;
    }

    return exp_table;
  }

  constexpr std::array<uint8_t, 256> make_log_table(
      const std::array<uint8_t, 256>& exp_table) const {
    std::array<uint8_t, 256> log_table{};
    for (size_t i = 0; i < log_table.size(); ++i) log_table[exp_table[i]] = static_cast<uint8_t>(i);
    return log_table;
  }

 private:
  size_t       m_qrWh_ = 0;
  QrCodePixels m_pixels_;

  std::array<uint8_t, 512> m_polyCoeffs_;
  size_t                   m_polyCoeffsSize_ = 0;

  std::vector<uint8_t> m_maskedPixels_;

  std::array<uint8_t, 256> m_expTable_ = make_exp_table();
  std::array<uint8_t, 256> m_logTable_ = make_log_table(m_expTable_);
};

#endif