#ifndef QRCODE_HPP
#define QRCODE_HPP

#include <algorithm>
#include <array>
#include <bitset>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <vector>

#include "qrcode_arrays.hpp"
#include "qrcode_pixels.hpp"

enum class Ecc : uint8_t { M = 0, L = 1, H = 2, Q = 3 };

static inline constexpr std::array<std::array<std::array<uint8_t, 2>, 4>, 40> c_qrEcTable{ {
    { { { 10, 1 }, { 7, 1 }, { 17, 1 }, { 13, 1 } } },
    { { { 16, 1 }, { 10, 1 }, { 28, 1 }, { 22, 1 } } },
    { { { 26, 1 }, { 15, 1 }, { 22, 2 }, { 18, 2 } } },
    { { { 18, 2 }, { 20, 1 }, { 16, 4 }, { 26, 2 } } },
    { { { 24, 2 }, { 26, 1 }, { 22, 4 }, { 18, 4 } } },
    { { { 16, 4 }, { 18, 2 }, { 28, 4 }, { 24, 4 } } },
    { { { 18, 4 }, { 20, 2 }, { 26, 5 }, { 18, 6 } } },
    { { { 22, 4 }, { 24, 2 }, { 26, 6 }, { 22, 6 } } },
    { { { 22, 5 }, { 30, 2 }, { 24, 8 }, { 20, 8 } } },
    { { { 26, 5 }, { 18, 4 }, { 28, 8 }, { 24, 8 } } },
    { { { 30, 5 }, { 20, 4 }, { 24, 11 }, { 28, 8 } } },
    { { { 22, 8 }, { 24, 4 }, { 28, 11 }, { 26, 10 } } },
    { { { 22, 9 }, { 26, 4 }, { 22, 16 }, { 24, 12 } } },
    { { { 24, 9 }, { 30, 4 }, { 24, 16 }, { 20, 16 } } },
    { { { 24, 10 }, { 22, 6 }, { 24, 18 }, { 30, 12 } } },
    { { { 28, 10 }, { 24, 6 }, { 30, 16 }, { 24, 17 } } },
    { { { 28, 11 }, { 28, 6 }, { 28, 19 }, { 28, 16 } } },
    { { { 26, 13 }, { 30, 6 }, { 28, 21 }, { 28, 18 } } },
    { { { 26, 14 }, { 28, 7 }, { 26, 25 }, { 26, 21 } } },
    { { { 26, 16 }, { 28, 8 }, { 28, 25 }, { 30, 20 } } },
    { { { 26, 17 }, { 28, 8 }, { 30, 25 }, { 28, 23 } } },
    { { { 28, 17 }, { 28, 9 }, { 24, 34 }, { 30, 23 } } },
    { { { 28, 18 }, { 30, 9 }, { 30, 30 }, { 30, 25 } } },
    { { { 28, 20 }, { 30, 10 }, { 30, 32 }, { 30, 27 } } },
    { { { 28, 21 }, { 26, 12 }, { 30, 35 }, { 30, 29 } } },
    { { { 28, 23 }, { 28, 12 }, { 30, 37 }, { 28, 34 } } },
    { { { 28, 25 }, { 30, 12 }, { 30, 40 }, { 30, 34 } } },
    { { { 28, 26 }, { 30, 13 }, { 30, 42 }, { 30, 35 } } },
    { { { 28, 28 }, { 30, 14 }, { 30, 45 }, { 30, 38 } } },
    { { { 28, 29 }, { 30, 15 }, { 30, 48 }, { 30, 40 } } },
    { { { 28, 31 }, { 30, 16 }, { 30, 51 }, { 30, 43 } } },
    { { { 28, 33 }, { 30, 17 }, { 30, 54 }, { 30, 45 } } },
    { { { 28, 35 }, { 30, 18 }, { 30, 57 }, { 30, 48 } } },
    { { { 28, 37 }, { 30, 19 }, { 30, 60 }, { 30, 51 } } },
    { { { 28, 38 }, { 30, 19 }, { 30, 63 }, { 30, 53 } } },
    { { { 28, 40 }, { 30, 20 }, { 30, 66 }, { 30, 56 } } },
    { { { 28, 43 }, { 30, 21 }, { 30, 70 }, { 30, 59 } } },
    { { { 28, 45 }, { 30, 22 }, { 30, 74 }, { 30, 62 } } },
    { { { 28, 47 }, { 30, 24 }, { 30, 77 }, { 30, 65 } } },
    { { { 28, 49 }, { 30, 25 }, { 30, 81 }, { 30, 68 } } },
} };

class CodewordBits {
 public:
  CodewordBits(size_t bit_len, const CodewordArray& codewords)
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
      if (m_wordIdx_ < m_codewords_.size)
        m_codewordBits_ = m_codewords_[m_wordIdx_];
    }

    // TODO: DEBUG m_alternate_ = !m_alternate_;
    return bit == 1;  // m_alternate_ ? 0 : 1;  // bit == 1;
  }

  bool at_last_bit() const {
    return m_bitIdx_ == 7;
  }

  bool in_bounds() const {
    return m_wordIdx_ != m_codewords_.size;
  }

 private:
  bool m_alternate_ = false;

  size_t m_bitLen_ = 0;
  size_t m_bitIdx_ = 0;
  size_t m_wordIdx_ = 0;

  const CodewordArray& m_codewords_;
  std::bitset<8>       m_codewordBits_;
};

class PerBlockInfo {
 public:
  PerBlockInfo(size_t g1_codewords_per_block, size_t ecc_codewords_per_block)
      : m_eccCodewords_{ ecc_codewords_per_block }
      , m_g1DataCodewords_{ g1_codewords_per_block }
      , m_g2DataCodewords_{ g1_codewords_per_block + 1 } {}

  size_t ec_codewords() const {
    return m_eccCodewords_;
  }

  size_t g1_data_codewords() const {
    return m_g1DataCodewords_;
  }

  size_t g2_data_codewords() const {
    return m_g2DataCodewords_;
  }

 private:
  size_t m_eccCodewords_{ 0 };
  size_t m_g1DataCodewords_{ 0 };
  size_t m_g2DataCodewords_{ 0 };
};

class G1G2BlocksInfo {
 public:
  G1G2BlocksInfo(size_t g1_blocks, size_t g2_blocks, PerBlockInfo&& per_block)
      : m_g1Blocks_{ g1_blocks }
      , m_g2Blocks_{ g2_blocks }
      , m_perBlock_{ per_block } {}

  const PerBlockInfo& per_block() const {
    return m_perBlock_;
  }

  size_t total_blocks() const {
    return m_g1Blocks_ + m_g2Blocks_;
  }

  size_t g1_blocks() const {
    return m_g1Blocks_;
  }

  size_t g2_blocks() const {
    return m_g2Blocks_;
  }

 private:
  size_t m_g1Blocks_{ 0 };
  size_t m_g2Blocks_{ 0 };

  PerBlockInfo m_perBlock_;
};

class QrCode {
 public:
  QrCode(const std::vector<uint8_t>& data, uint8_t version, Ecc ecc, bool debug = false)
      : m_qrWh_{ qrcode_wh(version) }
      , m_pixels_{ m_qrWh_ }
      , m_maskedPixels_(m_qrWh_ * m_qrWh_, 0) {
    assert(version != 0 && version <= 40);

    // Count of alignment + finder patterns
    const size_t patterns{ static_cast<size_t>(std::floor(static_cast<double>(version) / 7) + 2) };

    const size_t total_modules{ m_qrWh_ * m_qrWh_ };

    // TODO: Memoize!!

    constexpr size_t finder_pattern_modules = 3 * 8 * 8;
    const size_t alignment_pattern_modules{ version == 1 ? 0 : (patterns * patterns - 3) * 5 * 5 };
    const size_t timing_pattern_modules{ 2 * (4 * static_cast<size_t>(version) + 1) };
    const size_t intersection_modules{ version == 1 ? 0 : 2 * (patterns - 2) * 5 };

    constexpr size_t format_information_modules = 2 * 15;
    constexpr size_t version_information_modules = 2 * 3 * 6;

    // Subtracting from the total modules, the modules ...
    // - ... finder patterns
    // - ... alignment patterns
    // - ... timing patterns
    // - ... intersection of alignment and timing patterns
    // - ... the one and only always dark module
    // - ... the format information
    // - ... the version information (only for version 7 and above)
    // each take, resulting into the available data modules

    size_t data_modules = total_modules - finder_pattern_modules - alignment_pattern_modules -
                          timing_pattern_modules + intersection_modules - (1) -
                          format_information_modules -
                          (version >= 7 ? version_information_modules : 0);

    const std::array<uint8_t, 2>& ec_entry = c_qrEcTable[version - 1][static_cast<uint8_t>(ecc)];
    uint8_t                       total_blocks = ec_entry[1];
    size_t                        ec_codewords_per_block = ec_entry[0];
    size_t                        ec_codewords = ec_codewords_per_block * total_blocks;

    // Amount of bits each codeword takes
    uint8_t bit_len = codeword_bits(version);
    size_t  total_codewords = data_modules / bit_len;
    size_t  data_codewords = total_codewords - ec_codewords;

    // 1 + ´bit_len / 8´ bytes are reserved for the encoding mode (4 bits -> rounded 1 byte) and
    // data size (´bit_len´ bits, so for versions 9 and below, it's 1 byte, else 2 bytes)
    size_t encodeable_data_codewords = data_codewords - 1 - (bit_len / 8);

    // Compare bits: Because if the 'bit_len' is 16 bits then 2 bytes fit into 1 codeword
    if ((data.size() * 8) > (encodeable_data_codewords * bit_len)) {
      // TODO: Proper error handling
      std::cerr << "Too many bytes of data to encode!\n";
      m_pixels_.write_jpg();  // TODO: Remove
      return;
    }

    // Codewords per block for group 2 is always: codwords per block for group 1 + 1
    G1G2BlocksInfo g1g2_blocks_info{ 19, 6, PerBlockInfo{ 118, ec_codewords_per_block } };

    fill_codewords(data, data_codewords, total_codewords, 16);
    order_codewords(g1g2_blocks_info);

    polynomial_generator(ec_codewords_per_block);
    generate_ec_data(g1g2_blocks_info, data_codewords);

    if (debug) {
      std::cout << "Coeffs: [ ";
      for (size_t i = 0; i < m_polyCoeffs_.size; ++i) std::cout << m_polyCoeffs_[i] + 0 << " - ";
      std::cout << " ]";

      std::cout << "\n\n";

      std::cout << "Codewords (Binary): [ ";
      for (uint8_t c : m_codewords_) std::cout << std::bitset<8>(c) << " - ";
      std::cout << " ]";

      std::cout << "\n\n";

      std::cout << "Codewords (Decimal): [ ";
      for (uint8_t c : m_codewords_) std::cout << c + 0 << " - ";
      std::cout << " ]";

      std::cout << "\n\n";

      std::cout << "Ordered Codewords (Binary): [ ";
      for (uint8_t c : m_orderedCodewords_) std::cout << std::bitset<8>(c) << " - ";
      std::cout << " ]";

      std::cout << "\n\n";

      std::cout << "Ordered Codewords (Decimal): [ ";
      for (uint8_t c : m_orderedCodewords_) std::cout << c + 0 << " - ";
      std::cout << " ]";

      std::cout << "\n\n";
    }

    set_reserved_pixels(version);
    if (!debug)
      set_data_pixels(bit_len);

    size_t mask_idx = debug ? 0 : find_best_mask();
    set_format_info_modules(mask_idx, ecc, debug);
    if (version >= 7)
      set_version_info_modules(version, debug);

    if (!debug)
      mask_data_pixels(mask_idx);

    if (!debug) {
      for (size_t i = 0; i < m_maskedPixels_.size(); ++i)
        m_pixels_.pixels()[i] = (m_maskedPixels_[i] == 1 ? 0 : 255);
    }

    m_pixels_.write_jpg();
  }

 private:
  size_t qrcode_wh(uint8_t version) const {
    return version * 4 + 17;
  }

  uint8_t codeword_bits(uint8_t version) const {
    return version >= 10 ? 8 : 8;
  }

  void fill_codewords(const std::vector<uint8_t>& data, size_t data_codewords,
                      size_t total_codewords, size_t bit_len) {
    assert(m_codewords_.data.size() >= total_codewords);

    m_codewords_.size = total_codewords;

    uint8_t begin{ 0b01000000 };
    uint8_t low = static_cast<uint8_t>(data.size());
    uint8_t high = static_cast<uint8_t>(data.size() >> 8);
    begin |= high >> 4;

    m_codewords_[0] = begin;
    m_codewords_[1] = high << 4;

    size_t codeword_idx{ 1 };
    if (bit_len == 16) {
      codeword_idx = 2;
      m_codewords_[1] |= low >> 4;
      m_codewords_[2] = low << 4;
    }

    for (uint8_t b : data) {
      m_codewords_[codeword_idx++] |= b >> 4;
      assert(codeword_idx < data_codewords);
      m_codewords_[codeword_idx] = b << 4;
    }

    m_codewords_[codeword_idx++] &= 0b11110000;

    bool alternate{ false };
    while (codeword_idx < data_codewords) {
      m_codewords_[codeword_idx++] = alternate ? 0b00010001 : 0b11101100;
      alternate = !alternate;
    }
  }

  void order_codewords(const G1G2BlocksInfo& info) {
    if (info.g1_blocks() == 1) {
      for (size_t i = 0; i < m_codewords_.size; ++i) m_orderedCodewords_[i] = m_codewords_[i];
      m_orderedCodewords_.size = m_codewords_.size;
      return;
    }

    size_t g1_data_codewords_per_block = info.per_block().g1_data_codewords();
    size_t g2_data_codewords_per_block = info.per_block().g2_data_codewords();

    m_orderedCodewords_.size = m_codewords_.size;

    size_t ordered_codewords_idx = 0;
    for (size_t i = 0; i < g1_data_codewords_per_block; ++i) {
      size_t codeword_idx = 0;
      for (size_t b = 0; b < info.total_blocks(); ++b) {
        bool g1 = b < info.g1_blocks();
        m_orderedCodewords_[ordered_codewords_idx++] = m_codewords_[codeword_idx + i];
        codeword_idx += g1 ? g1_data_codewords_per_block : g2_data_codewords_per_block;
      }
    }

    if (info.g2_blocks() == 0)
      return;

    size_t codeword_idx = (info.g1_blocks() + 1) * g1_data_codewords_per_block;
    for (size_t b = 0; b < info.g2_blocks(); ++b) {
      m_orderedCodewords_[ordered_codewords_idx++] = m_codewords_[codeword_idx];
      codeword_idx += g2_data_codewords_per_block;
    }
  }

  void generate_ec_data(const G1G2BlocksInfo& info, size_t data_codewords) {
    size_t ec_codewords_per_block = info.per_block().ec_codewords();

    size_t g1_data_codewords_per_block = info.per_block().g1_data_codewords();
    size_t g1_total_codewords_per_block = g1_data_codewords_per_block + ec_codewords_per_block;

    size_t g2_data_codewords_per_block = info.per_block().g2_data_codewords();
    size_t g2_total_codewords_per_block = g2_data_codewords_per_block + ec_codewords_per_block;

    size_t blocks = info.total_blocks();
    size_t g1_blocks = info.g1_blocks();

    size_t offset = 0;

    for (size_t i = 0; i < blocks; ++i) {
      bool g1 = i < g1_blocks;

      size_t data_codewords_per_block =
          g1 ? g1_data_codewords_per_block : g2_data_codewords_per_block;
      size_t total_codewords_per_block =
          g1 ? g1_total_codewords_per_block : g2_total_codewords_per_block;

      set_polynomial_division_input_codewords(data_codewords_per_block, total_codewords_per_block,
                                              offset);
      polynomial_division_ordered_codewords(data_codewords_per_block, total_codewords_per_block);

      for (size_t c = 0; c < ec_codewords_per_block; ++c)
        m_orderedCodewords_[(c * blocks + i) + data_codewords] =
            m_polyDivInput_[c + data_codewords_per_block];

      offset += data_codewords_per_block;
    }
  }

  void set_data_pixels(uint8_t bit_len) {
    bool   up_dir{ true };
    size_t cur_row{ m_qrWh_ - 1 };
    // TODO: Address?
    int32_t cur_col{ static_cast<int32_t>(m_qrWh_) };

    CodewordBits bits{ bit_len, m_orderedCodewords_ };

    m_pixels_.fill_pixel(cur_row, --cur_col, bits.get());
    m_pixels_.fill_pixel(cur_row, --cur_col, bits.get());

    bool going_up = true;
    bool may_move_left = false;

    auto check_row_availability = [&](bool assert_if_not_found = false) {
      if (going_up) {
        for (size_t r = cur_row; r-- != 0;) {
          if (!m_pixels_.is_reserved(r, cur_col)) {
            cur_row = r + 1;
            return true;
          }

          if (r == 0) {
            assert(!assert_if_not_found);

            if (!m_pixels_.is_reserved(r, cur_col - 2)) {
              cur_row = r;
              cur_col -= 2;
              m_pixels_.fill_pixel(cur_row, cur_col, bits.get());

              going_up = false;
              return true;
            }
          }
        }
      } else {
        for (size_t r = cur_row + 1; r < m_qrWh_; ++r) {
          if (!m_pixels_.is_reserved(r, cur_col)) {
            cur_row = r - 1;
            return true;
          }

          if (r == m_qrWh_ - 1) {
            assert(!assert_if_not_found);

            if (!m_pixels_.is_reserved(r, cur_col - 2)) {
              cur_row = r;
              cur_col -= 2;
              m_pixels_.fill_pixel(cur_row, cur_col, bits.get());

              going_up = true;
              return true;
            }
          }
        }
      }

      return false;
    };

    while (bits.in_bounds()) {
      if (may_move_left)
        m_pixels_.fill_pixel(cur_row, --cur_col, bits.get());
      may_move_left = true;

      if (!bits.in_bounds())
        break;

      size_t next_row = going_up ? cur_row - 1 : cur_row + 1;

      bool next_row_in_bounds =
          (going_up && (cur_row > 0)) || (!going_up && ((cur_row + 1) < m_qrWh_));

      bool can_move_to_next_row = next_row_in_bounds && !m_pixels_.is_reserved(next_row, cur_col);
      bool can_move_diagonal =
          can_move_to_next_row && !m_pixels_.is_reserved(next_row, cur_col + 1);

      if (can_move_diagonal) {
        m_pixels_.fill_pixel(next_row, ++cur_col, bits.get());
        cur_row = next_row;
        continue;
      }

      if (can_move_to_next_row) {
        m_pixels_.fill_pixel(next_row, cur_col, bits.get());
        cur_row = next_row;
        may_move_left = false;
        continue;
      }

      if (check_row_availability()) {
        may_move_left = false;
        continue;
      }

      --cur_col;
      bool can_move_left = !m_pixels_.is_reserved(cur_row, cur_col);

      if (can_move_left)
        m_pixels_.fill_pixel(cur_row, cur_col, bits.get());
      else {
        if (!m_pixels_.is_reserved(cur_row, cur_col - 1)) {
          --cur_col;
          m_pixels_.fill_pixel(cur_row, cur_col, bits.get());
        } else {
          going_up = !going_up;
          check_row_availability(true);
          m_pixels_.fill_pixel(--cur_row, cur_col, bits.get());
          continue;
        }
      }

      going_up = !going_up;
    }

    while (bits.in_bounds()) {
      m_pixels_.fill_pixel(cur_row, --cur_col, bits.get());

      if (!bits.in_bounds())
        break;

      bool is_last_row = (up_dir && cur_row <= 0) || (!up_dir && cur_row >= m_qrWh_ - 1);

      if (!is_last_row) {
        bool   stop = false;
        size_t next_row{ up_dir ? cur_row - 1 : cur_row + 1 };
        while (!m_pixels_.is_reserved(next_row, cur_col) &&
               m_pixels_.is_reserved(next_row, cur_col + 1)) {
          up_dir ? --cur_row : ++cur_row;
          next_row = up_dir ? cur_row - 1 : cur_row + 1;
          m_pixels_.fill_pixel(cur_row, cur_col, bits.get());

          if (!bits.in_bounds())
            break;
        }

        if (!bits.in_bounds())
          break;

        is_last_row = true;
        if (up_dir) {
          for (size_t r = cur_row; r-- != 0;) {
            if (!m_pixels_.is_reserved(r, cur_col)) {
              cur_row = r;
              is_last_row = false;
              break;
            }

            if (r == 0) {
              if (!m_pixels_.is_reserved(r, cur_col + 1)) {
                cur_row = r;
                cur_col += 1;
                is_last_row = false;
                break;
              }

              if (!m_pixels_.is_reserved(r, cur_col + 2)) {
                cur_row = r;
                cur_col += 2;
                is_last_row = false;
                break;
              }
            }
          }
        } else {
          for (size_t r = cur_row + 1; r < m_qrWh_; ++r) {
            if (!m_pixels_.is_reserved(r, cur_col)) {
              cur_row = r;
              is_last_row = false;
              break;
            }

            if (r == m_qrWh_ - 1) {
              if (!m_pixels_.is_reserved(r, cur_col + 1)) {
                cur_row = r;
                cur_col = cur_col + 1;
                is_last_row = false;
                break;
              }

              if (!m_pixels_.is_reserved(r, cur_col + 2)) {
                cur_row = r;
                cur_col = cur_col + 2;
                is_last_row = false;
                break;
              }
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

    struct PatternArea {
      size_t row{ 0 };
      size_t col{ 0 };
      size_t width{ 0 };
      size_t height{ 0 };
      size_t finder_width{ 0 };
      size_t finder_height{ 0 };
    };

    // Finder patterns
    std::vector<PatternArea> pattern_areas{
      PatternArea{
          .row = 0, .col = 0, .width = 7, .height = 7, .finder_width = 9, .finder_height = 9 },
      PatternArea{ .row = 0,
                   .col = m_qrWh_ - 7,
                   .width = 7,
                   .height = 7,
                   .finder_width = 8,
                   .finder_height = 9 },
      PatternArea{ .row = m_qrWh_ - 7,
                   .col = 0,
                   .width = 7,
                   .height = 7,
                   .finder_width = 9,
                   .finder_height = 8 },
    };

    // Version 1 doesn't have any alignment patterns
    if (version > 1) {
      // Reserving Alignment patterns
      size_t pattern_count = std::floor(static_cast<double>(version) / 7) + 2;
      pattern_areas.reserve(pattern_count);

      size_t first_pattern_column = 6;
      size_t last_pattern_column = m_qrWh_ - 6 - 1;

      size_t bottom_row = m_qrWh_ - 9;

      size_t dist_between_patterns =
          (last_pattern_column - first_pattern_column) / (pattern_count - 1);
      dist_between_patterns =
          (dist_between_patterns + 2 - 1) - ((dist_between_patterns + 2 - 1) % 2);

      for (int r = 0; r < pattern_count; ++r) {
        size_t last_pattern = pattern_count - 1;
        size_t row = r == last_pattern ? 4 : bottom_row - (dist_between_patterns * r);

        bool is_first_row = r == 0;
        bool is_last_row = r == last_pattern;

        for (size_t c = 0; c < pattern_count; ++c) {
          // Skip finder patterns
          if ((c == last_pattern && (is_last_row || is_first_row)) || c == 0 && is_last_row)
            continue;

          size_t column = c == 0              ? last_pattern_column
                          : c == last_pattern ? 6
                                              : last_pattern_column - (dist_between_patterns * c);
          m_pixels_.fill_area(row, column - 2, 5, 5, true, true);
          pattern_areas.emplace_back(row, column - 2, 5, 5);
        }
      }
    }

    // Reserving timing patterns
    m_pixels_.fill_area(6, 9, version * 4, 1, true, true);
    m_pixels_.fill_area(9, 6, 1, version * 4, true, true);

    // Make each pattern actually an qr code pattern, seperating the inner dark square from the
    // outer dark square
    for (const PatternArea& area : pattern_areas) {
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

    // Make the timing patterns actually alternate between dark and light modules
    m_pixels_.fill_area(6, 9, version * 4, 1, false, true);
    m_pixels_.fill_area(9, 6, 1, version * 4, false, true);

    for (size_t i = 8; i < 10 + version * 4; i += 2) m_pixels_.fill_pixel(6, i, true, true);
    for (size_t i = 8; i < 10 + version * 4; i += 2) m_pixels_.fill_pixel(i, 6, true, true);

    // Versions 7 and below don't have version information
    if (version >= 7) {
      // Reserving version information modules
      m_pixels_.fill_area(m_qrWh_ - 11, 0, 6, 3, true, true);
      m_pixels_.fill_area(0, m_qrWh_ - 11, 3, 6, true, true);
    }

    // Reserving dark module
    m_pixels_.fill_pixel(m_qrWh_ - 8, 8, true, true);
  }

  void set_format_info_modules(uint8_t mask_idx, Ecc ecc, bool debug = false) {
    const uint8_t ecc_idx{ static_cast<uint8_t>(ecc) };

    BchArray format_info;
    format_info[0] = ecc_idx >> 1;
    format_info[1] = ecc_idx & 1;
    format_info[2] = mask_idx >> 2;
    format_info[3] = (mask_idx >> 1) & 1;
    format_info[4] = mask_idx & 1;
    format_info.size = 15;

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
    m_polyCoeffs_.size = 11;

    static BchArray debug_data;
    debug_data[0] = 0;
    debug_data[1] = 18;
    debug_data[2] = 36;
    debug_data[3] = 54;
    debug_data[4] = 73;
    debug_data[5] = 91;
    debug_data[6] = 109;
    debug_data[7] = 128;
    debug_data[8] = 146;
    debug_data[9] = 164;
    debug_data[10] = 182;
    debug_data[11] = 201;
    debug_data[12] = 219;
    debug_data[13] = 237;
    debug_data[14] = 255;
    debug_data.size = 15;

    if (debug) {
      std::cout << "Format Info BCH (Pre-Operations): [ ";
      for (size_t i = 0; i < format_info.size; ++i) std::cout << format_info[i] + 0 << " - ";
      std::cout << " ]\n\n";
    }

    bch(format_info, 5);

    if (debug) {
      std::cout << "Format Info BCH (Post-Operation): [ ";
      for (size_t i = 0; i < format_info.size; ++i) std::cout << format_info[i] + 0 << " - ";
      std::cout << " ]\n\n";
    }

    static constexpr std::array<uint8_t, 15> format_mask{ 1, 0, 1, 0, 1, 0, 0, 0,
                                                          0, 0, 1, 0, 0, 1, 0 };

    for (size_t i = 0; i < format_info.size; ++i) format_info[i] ^= format_mask[i];

    if (debug) {
      std::cout << "Format Info BCH (Post-Mask): [ ";
      for (size_t i = 0; i < format_info.size; ++i) std::cout << format_info[i] + 0 << " - ";
      std::cout << " ]\n\n";
    }

    // fill_area with custom data inverts the rows min/max!!

    m_pixels_.fill_area(m_qrWh_ - 9, 0, 6, 1, debug ? debug_data : format_info, 0, debug);
    m_pixels_.fill_area(m_qrWh_ - 9, 7, 1, 1, debug ? debug_data : format_info, 6, debug);
    m_pixels_.fill_area(m_qrWh_ - 9, 8, 1, 2, debug ? debug_data : format_info, 7, debug);
    m_pixels_.fill_area(m_qrWh_ - 6, 8, 1, 6, debug ? debug_data : format_info, 9, debug);

    m_pixels_.fill_area(0, 8, 1, 7, debug ? debug_data : format_info, 0, debug);
    m_pixels_.fill_area(m_qrWh_ - 9, m_qrWh_ - 8, 8, 1, debug ? debug_data : format_info, 7, debug);
  }

  void set_version_info_modules(uint8_t version, bool debug = false) {
    BchArray version_info;
    version_info[0] = (version >> 5) & 1;
    version_info[1] = (version >> 4) & 1;
    version_info[2] = (version >> 3) & 1;
    version_info[3] = (version >> 2) & 1;
    version_info[4] = (version >> 1) & 1;
    version_info[5] = version & 1;
    version_info.size = 18;

    m_polyCoeffs_[0] = 1;
    m_polyCoeffs_[1] = 1;
    m_polyCoeffs_[2] = 1;
    m_polyCoeffs_[3] = 1;
    m_polyCoeffs_[4] = 1;
    m_polyCoeffs_[5] = 0;
    m_polyCoeffs_[6] = 0;
    m_polyCoeffs_[7] = 1;
    m_polyCoeffs_[8] = 0;
    m_polyCoeffs_[9] = 0;
    m_polyCoeffs_[10] = 1;
    m_polyCoeffs_[11] = 0;
    m_polyCoeffs_[12] = 1;
    m_polyCoeffs_.size = 13;

    static BchArray debug_data;
    debug_data[0] = 0;
    debug_data[1] = 15;
    debug_data[2] = 30;
    debug_data[3] = 45;
    debug_data[4] = 60;
    debug_data[5] = 75;
    debug_data[6] = 90;
    debug_data[7] = 105;
    debug_data[8] = 120;
    debug_data[9] = 135;
    debug_data[10] = 150;
    debug_data[11] = 165;
    debug_data[12] = 180;
    debug_data[13] = 195;
    debug_data[14] = 210;
    debug_data[15] = 225;
    debug_data[16] = 240;
    debug_data[17] = 255;
    debug_data.size = 18;

    if (debug) {
      std::cout << "Version BCH (Pre-Operation): [ ";
      for (size_t i = 0; i < version_info.size; ++i) std::cout << version_info[i] + 0 << " - ";
      std::cout << " ]\n\n";
    }

    bch(version_info, 6);

    if (debug) {
      std::cout << "Version BCH (Post-Operation): [ ";
      for (size_t i = 0; i < version_info.size; ++i) std::cout << version_info[i] + 0 << " - ";
      std::cout << " ]\n\n";
    }

    std::reverse(version_info.begin(), version_info.end());
    // fill_area with custom data inverts the rows min/max!!

    m_pixels_.fill_area(8, 0, 1, 3, debug ? debug_data : version_info, 0, debug, true);
    m_pixels_.fill_area(8, 1, 1, 3, debug ? debug_data : version_info, 3, debug, true);
    m_pixels_.fill_area(8, 2, 1, 3, debug ? debug_data : version_info, 6, debug, true);
    m_pixels_.fill_area(8, 3, 1, 3, debug ? debug_data : version_info, 9, debug, true);
    m_pixels_.fill_area(8, 4, 1, 3, debug ? debug_data : version_info, 12, debug, true);
    m_pixels_.fill_area(8, 5, 1, 3, debug ? debug_data : version_info, 15, debug, true);

    m_pixels_.fill_area(m_qrWh_ - 1, m_qrWh_ - 11, 3, 1, debug ? debug_data : version_info, 0,
                        debug);
    m_pixels_.fill_area(m_qrWh_ - 2, m_qrWh_ - 11, 3, 1, debug ? debug_data : version_info, 3,
                        debug);
    m_pixels_.fill_area(m_qrWh_ - 3, m_qrWh_ - 11, 3, 1, debug ? debug_data : version_info, 6,
                        debug);
    m_pixels_.fill_area(m_qrWh_ - 4, m_qrWh_ - 11, 3, 1, debug ? debug_data : version_info, 9,
                        debug);
    m_pixels_.fill_area(m_qrWh_ - 5, m_qrWh_ - 11, 3, 1, debug ? debug_data : version_info, 12,
                        debug);
    m_pixels_.fill_area(m_qrWh_ - 6, m_qrWh_ - 11, 3, 1, debug ? debug_data : version_info, 15,
                        debug);
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

    for (size_t i = 1; i < m_polyCoeffs_.size; ++i) {
      uint8_t cur = m_polyCoeffs_[i];

      if (cur == 0) {
        m_polyCoeffs_[i] = cur;
        continue;
      }

      uint8_t matched = gf256_mul(prev_element, factor);
      prev_element = cur;
      m_polyCoeffs_[i] = cur ^ matched;
    }

    m_polyCoeffs_[m_polyCoeffs_.size] = gf256_mul(prev_element, factor);
    ++m_polyCoeffs_.size;
  }

  void polynomial_generator(uint8_t degree) {
    assert(degree > 0);
    m_polyCoeffs_[0] = 1;
    m_polyCoeffs_.size = 1;
    for (uint8_t i = 0; i < degree; ++i) polynomial_multiplication(m_expTable_[i]);
  }

  void bch(BchArray& out, size_t data_size) {
    assert(out.size >= data_size);

    for (size_t i = 0; i < out.size; ++i) m_polyDivInput_[i] = out[i];
    m_polyDivInput_.size = out.size;

    for (size_t i = 0; i < data_size; ++i) {
      uint8_t start{ m_polyDivInput_[i] };
      if (start != 0)
        for (size_t g = 0; g < m_polyCoeffs_.size; ++g) m_polyDivInput_[i + g] ^= m_polyCoeffs_[g];
    }
    for (size_t i = data_size; i < out.size; ++i) out[i] = m_polyDivInput_[i];
  }

  void polynomial_division_ordered_codewords(size_t data_codewords_per_block,
                                             size_t total_codewords_per_block) {
    assert(m_polyDivInput_.size == total_codewords_per_block);

    for (size_t i = 0; i < data_codewords_per_block; ++i) {
      uint8_t start{ m_polyDivInput_[i] };
      if (start != 0) {
        for (size_t g = 0; g < m_polyCoeffs_.size; ++g)
          m_polyDivInput_[i + g] ^= gf256_mul(start, m_polyCoeffs_[g]);
      }
    }
  }

  void set_polynomial_division_input_codewords(size_t data_codewords_per_block,
                                               size_t total_codewords_per_block, size_t offset) {
    assert(m_codewords_.size >= offset);
    for (size_t i{ 0 }; i < total_codewords_per_block; ++i)
      m_polyDivInput_[i] = i >= data_codewords_per_block ? 0 : m_codewords_[i + offset];
    m_polyDivInput_.size = total_codewords_per_block;
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
  // TODO: maybe also rename to something like: m_size_
  size_t m_qrWh_ = 0;
  // TODO: wrong naming: should be modules, instead of pixels
  QrCodePixels         m_pixels_;
  std::vector<uint8_t> m_maskedPixels_;

  // TODO: whats the max?
  CodewordArray m_codewords_{};
  CodewordArray m_orderedCodewords_{};

  // TODO: make sure, that no qr code can exceed this array
  StackArray<uint8_t, 512> m_polyCoeffs_;
  StackArray<uint8_t, 512> m_polyDivInput_{};

  std::array<uint8_t, 256> m_expTable_{ make_exp_table() };
  std::array<uint8_t, 256> m_logTable_{ make_log_table(m_expTable_) };
};

#endif