// SPDX-FileCopyrightText: 2026 Julian Duwe
// SPDX-License-Identifier: Apache-2.0

#ifndef JLD_QRCODE_MEMOIZER
#define JLD_QRCODE_MEMOIZER

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <exception>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_set>

#include "qrcode/common.hpp"

namespace jld {

// Memoizes common pre-known data for every QR-Code version specified and stores the tables into a
// file (C++ syntax). First and foremost, I hate generating code... it just looks wrong
class QrCodeMemoizer {
 public:
  explicit QrCodeMemoizer(const std::string& filepath)
      : m_fileStream_{ filepath, std::ios::out }
      , m_isValid_{ m_fileStream_.is_open() } {
    if (!m_fileStream_.is_open()) {
      std::cerr << "[ERROR] jldQrCodeMemoizer -> Failed to open file: " << filepath << ".\n";
      return;
    }

    // REUSE-IgnoreStart

    m_fileStream_
        << "// SPDX-FileCopyrightText: 2026 Julian Duwe"
           "\n// SPDX-License-Identifier: Apache-2.0"
           "\n\n// Memoized QR-Code data tables for https://www.github.com/jld-wk/qrcode-cpp"
           "\n// Generated using "
           "https://www.github.com/jld-wk/qrcode-cpp/blob/development/tools/memoizer.hpp\n"
           "\n// Do not edit!\n"
           "\n#ifndef JLD_QRCODE_MEMOIZED"
           "\n#define JLD_QRCODE_MEMOIZED\n"
           "\n#include <cstdint>"
           "\n#include <array>\n"
           "\nnamespace jld {\n";

    // REUSE-IgnoreEnd

    generate_exp_table();
    generate_log_table();

    generate_encoding_info();
    generate_poly_generator();

    generate_format_info();
    generate_version_info();

    m_fileStream_ << "\n}  // namespace jld\n"
                     "\n#endif // JLD_QRCODE_MEMOIZED";
  }

  auto successful() const -> bool {
    return m_isValid_;
  }

 private:
  void generate_exp_table() {
    if (!m_isValid_)
      return;

    uint8_t prev_res{ 1 };

    for (size_t i = 1; i < m_expTable_.size(); ++i) {
      if (prev_res > 127) {
        uint8_t res = prev_res;
        res <<= 1;
        if (prev_res & 0b10000000)
          res ^= 0b00011101;
        prev_res = res;
        m_expTable_[i] = res;
        continue;
      }

      uint8_t res = prev_res * 2;
      m_expTable_[i] = res;
      prev_res = res;
    }

    std::stringstream ss;
    ss << "\nstatic inline constexpr std::array<uint8_t, 256> "
          "c_expTable{\n    ";

    for (size_t i = 0; i < m_expTable_.size(); ++i) {
      ss << static_cast<size_t>(m_expTable_[i]);
      if (i != 255)
        ss << ", ";
    }

    ss << "\n};\n";
    m_fileStream_ << ss.str();

    m_expTableGenerated_ = true;
  }

  void generate_log_table() {
    if (!m_isValid_)
      return;

    if (!m_expTableGenerated_) {
      std::cerr
          << "[ERROR] jldQrCodeMemoizer -> The logarithmic table requires the exponential table, "
             "generate it first.\n";
      return;
    }

    for (size_t i = 0; i < m_logTable_.size(); ++i)
      m_logTable_[m_expTable_[i]] = static_cast<uint8_t>(i);

    std::stringstream ss;
    ss << "\nstatic inline constexpr std::array<uint8_t, 256> "
          "c_logTable{\n    ";

    for (size_t i = 0; i < m_logTable_.size(); ++i) {
      ss << static_cast<size_t>(m_logTable_[i]);
      if (i != 255)
        ss << ", ";
    }

    ss << "\n};\n";
    m_fileStream_ << ss.str();

    m_logTableGenerated_ = true;
  }

  void generate_encoding_info() {
    if (!m_isValid_)
      return;

    /*
      Structured, like:
      V1-1   M { EC/block, G1 blocks, G1 data/block, G2 blocks  } L { ... } H { ... } Q { ... }
      V2-1   ... ->
      V3-1   ... ->
      ||
      \/

      I know the order doesn't make any sense at first glance, as L is the lowest error correction
      level and H the highest, but don't blame me, it's the specification, M is index 0 and Q is
      index 3. But I mean, it's for a good reason to increase readability and stuff bla bla.
    */
    constexpr std::array<std::array<std::array<uint8_t, 4>, 4>, 40> qr_block_table{ {
        { { { 10, 1, 16, 0 }, { 7, 1, 19, 0 }, { 17, 1, 9, 0 }, { 13, 1, 13, 0 } } },
        { { { 16, 1, 28, 0 }, { 10, 1, 34, 0 }, { 28, 1, 16, 0 }, { 22, 1, 22, 0 } } },
        { { { 26, 1, 44, 0 }, { 15, 1, 55, 0 }, { 22, 2, 13, 0 }, { 18, 2, 17, 0 } } },
        { { { 18, 2, 32, 0 }, { 20, 1, 80, 0 }, { 16, 4, 9, 0 }, { 26, 2, 24, 0 } } },
        { { { 24, 2, 43, 0 }, { 26, 1, 108, 0 }, { 22, 2, 11, 2 }, { 18, 2, 15, 2 } } },
        { { { 16, 4, 27, 0 }, { 18, 2, 68, 0 }, { 28, 4, 15, 0 }, { 24, 4, 19, 0 } } },
        { { { 18, 4, 31, 0 }, { 20, 2, 78, 0 }, { 26, 4, 13, 1 }, { 18, 2, 14, 4 } } },
        { { { 22, 2, 38, 2 }, { 24, 2, 97, 0 }, { 26, 4, 14, 2 }, { 22, 4, 18, 2 } } },
        { { { 22, 3, 36, 2 }, { 30, 2, 116, 0 }, { 24, 4, 12, 4 }, { 20, 4, 16, 4 } } },
        { { { 26, 4, 43, 1 }, { 18, 2, 68, 2 }, { 28, 6, 15, 2 }, { 24, 6, 19, 2 } } },
        { { { 30, 1, 50, 4 }, { 20, 4, 81, 0 }, { 24, 3, 12, 8 }, { 28, 4, 22, 4 } } },
        { { { 22, 6, 36, 2 }, { 24, 2, 92, 2 }, { 28, 7, 14, 4 }, { 26, 4, 20, 6 } } },
        { { { 22, 8, 37, 1 }, { 26, 4, 107, 0 }, { 22, 12, 11, 4 }, { 24, 8, 20, 4 } } },
        { { { 24, 4, 40, 5 }, { 30, 3, 115, 1 }, { 24, 11, 12, 5 }, { 20, 11, 16, 5 } } },
        { { { 24, 5, 41, 5 }, { 22, 5, 87, 1 }, { 24, 11, 12, 7 }, { 30, 5, 24, 7 } } },
        { { { 28, 7, 45, 3 }, { 24, 5, 98, 1 }, { 30, 3, 15, 13 }, { 24, 15, 19, 2 } } },
        { { { 28, 10, 46, 1 }, { 28, 1, 107, 5 }, { 28, 2, 14, 17 }, { 28, 1, 22, 15 } } },
        { { { 26, 9, 43, 4 }, { 30, 5, 120, 1 }, { 28, 2, 14, 19 }, { 28, 17, 22, 1 } } },
        { { { 26, 3, 44, 11 }, { 28, 3, 113, 4 }, { 26, 9, 13, 16 }, { 26, 17, 21, 4 } } },
        { { { 26, 3, 41, 13 }, { 28, 3, 107, 5 }, { 28, 15, 15, 10 }, { 30, 15, 24, 5 } } },
        { { { 26, 17, 42, 0 }, { 28, 4, 116, 4 }, { 30, 19, 16, 6 }, { 28, 17, 22, 6 } } },
        { { { 28, 17, 46, 0 }, { 28, 2, 111, 7 }, { 24, 34, 13, 0 }, { 30, 7, 24, 16 } } },
        { { { 28, 4, 47, 14 }, { 30, 4, 121, 5 }, { 30, 16, 15, 14 }, { 30, 11, 24, 14 } } },
        { { { 28, 6, 45, 14 }, { 30, 6, 117, 4 }, { 30, 30, 16, 2 }, { 30, 11, 24, 16 } } },
        { { { 28, 8, 47, 13 }, { 26, 8, 106, 4 }, { 30, 22, 15, 13 }, { 30, 7, 24, 22 } } },
        { { { 28, 19, 46, 4 }, { 28, 10, 114, 2 }, { 30, 33, 16, 4 }, { 28, 28, 22, 6 } } },
        { { { 28, 22, 45, 3 }, { 30, 8, 122, 4 }, { 30, 12, 15, 28 }, { 30, 8, 23, 26 } } },
        { { { 28, 3, 45, 23 }, { 30, 3, 117, 10 }, { 30, 11, 15, 31 }, { 30, 4, 24, 31 } } },
        { { { 28, 21, 45, 7 }, { 30, 7, 116, 7 }, { 30, 19, 15, 26 }, { 30, 1, 23, 37 } } },
        { { { 28, 19, 47, 10 }, { 30, 5, 115, 10 }, { 30, 23, 15, 25 }, { 30, 15, 24, 25 } } },
        { { { 28, 2, 46, 29 }, { 30, 13, 115, 3 }, { 30, 23, 15, 28 }, { 30, 42, 24, 1 } } },
        { { { 28, 10, 46, 23 }, { 30, 17, 115, 0 }, { 30, 19, 15, 35 }, { 30, 10, 24, 35 } } },
        { { { 28, 14, 46, 21 }, { 30, 17, 115, 1 }, { 30, 11, 15, 46 }, { 30, 29, 24, 19 } } },
        { { { 28, 14, 46, 23 }, { 30, 13, 115, 6 }, { 30, 59, 16, 1 }, { 30, 44, 24, 7 } } },
        { { { 28, 12, 47, 26 }, { 30, 12, 121, 7 }, { 30, 22, 15, 41 }, { 30, 39, 24, 14 } } },
        { { { 28, 6, 47, 34 }, { 30, 6, 121, 14 }, { 30, 2, 15, 64 }, { 30, 46, 24, 10 } } },
        { { { 28, 29, 46, 14 }, { 30, 17, 122, 4 }, { 30, 24, 15, 46 }, { 30, 49, 24, 10 } } },
        { { { 28, 13, 46, 32 }, { 30, 4, 122, 18 }, { 30, 42, 15, 32 }, { 30, 48, 24, 14 } } },
        { { { 28, 40, 47, 7 }, { 30, 20, 117, 4 }, { 30, 10, 15, 67 }, { 30, 43, 24, 22 } } },
        { { { 28, 18, 47, 31 }, { 30, 19, 118, 6 }, { 30, 20, 15, 61 }, { 30, 34, 24, 34 } } },
    } };

    std::stringstream ss;
    ss << "\nstruct QrCodeEncodingInfo {"
          "\nuint32_t modulesCount;"
          "\nuint32_t modulesWidth;"
          "\nuint8_t  bitLen;"
          "\nuint8_t  g1BlocksCount;"
          "\nuint8_t  g2BlocksCount;"
          "\nuint32_t ecCodewordsCount;"
          "\nuint8_t  g1g2EcCodewordsCount;"
          "\nuint8_t  g1DataCodewordsCount;"
          "\nuint32_t dataCodewordsCount;"
          "\nuint32_t writableDataCodewordsCount;"
          "\n};\n"
          "\n// Structured, like:"
          "\n// V1-1   M { Encoding info } L { ... } H { ... } Q { ... }"
          "\n// V2-1   ... ->"
          "\n// V3-1   ... ->"
       << "\nstatic inline constexpr std::array<std::array<QrCodeEncodingInfo, 4>, 40> "
          "c_qrEncodingInfoTable{{";

    uint32_t largest_modules_count = 0;
    uint32_t largest_codewords_count = 0;
    uint8_t  largest_g1g2_codewords_count = 0;
    uint8_t  largest_g1g2_ec_codewords_count = 0;

    // Generate encoding info for version 1-40 and error correction level 1-4
    for (uint8_t v = 1; v < 41; ++v) {
      ss << "{{";

      for (uint8_t e = 0; e < 4; ++e) {
        const uint32_t patterns_per_row{ static_cast<uint32_t>(
            std::floor(static_cast<double>(v) / 7) + 2) };

        // QRCode size: 'modules_width' x 'modules_width'
        const uint32_t modules_width = (v * 4) + 17;
        // The amount of bits to use to encode the size of the data to encode
        const uint8_t bit_len = v >= 10 ? 16 : 8;

        const uint32_t modules_count{ modules_width * modules_width };

        // The amount of modules each reserved area requires: \/
        constexpr uint32_t finder_pattern_modules = 3 * 8 * 8;
        const uint32_t     alignment_pattern_modules{
          v == 1 ? 0 : (patterns_per_row * patterns_per_row - 3) * 5 * 5
        };
        const uint32_t timing_pattern_modules{ 2 * (4 * static_cast<uint32_t>(v) + 1) };
        const uint32_t intersection_modules{ patterns_per_row == 1
                                                 ? 0
                                                 : 2 * (patterns_per_row - 2) * 5 };

        constexpr uint32_t format_info_modules = 2 * 15;
        constexpr uint32_t version_info_modules = 2 * 18;

        // Subtracting from the total modules, the modules ...
        // - ... finder patterns
        // - ... alignment patterns
        // - ... timing patterns
        // - ... intersection of alignment and timing patterns
        // - ... the one and only always dark module
        // - ... the format information
        // - ... the version information (only for version 7 and above)
        // each take, resulting into the available data modules

        const uint32_t data_modules = modules_count - finder_pattern_modules -
                                      alignment_pattern_modules - timing_pattern_modules +
                                      intersection_modules - (1) - format_info_modules -
                                      (v >= 7 ? version_info_modules : 0);

        const uint8_t                version_idx = v - 1;
        const std::array<uint8_t, 4> table_entry = qr_block_table[version_idx][e];

        const uint8_t g1g2_ec_codewords_count = table_entry[0];
        const uint8_t g1_blocks_count = table_entry[1];
        const uint8_t g1_data_codewords_count = table_entry[2];
        const uint8_t g2_blocks_count = table_entry[3];

        const uint32_t blocks_count = g1_blocks_count + g2_blocks_count;
        const uint32_t ec_codewords_count = g1g2_ec_codewords_count * blocks_count;

        constexpr uint32_t byte_in_bits = 8;

        const uint32_t codewords_count = data_modules / byte_in_bits;
        const uint32_t data_codewords_count = codewords_count - ec_codewords_count;

        // 2 or 3 bytes (depending on the version) are reserved for the encoding mode (4
        // bits -> rounded 1 byte) and data size (2 bytes for versions 10 and above and 1 byte for
        // any version below 10)
        const uint32_t writable_data_codewords_count =
            data_codewords_count - 1 - (bit_len / byte_in_bits);

        ss << "{.modulesCount=" << modules_count << ", .modulesWidth=" << modules_width
           << ", .bitLen=" << static_cast<size_t>(bit_len)
           << ", .g1BlocksCount=" << static_cast<size_t>(g1_blocks_count)
           << ", .g2BlocksCount=" << static_cast<size_t>(g2_blocks_count)
           << ", .ecCodewordsCount=" << ec_codewords_count
           << ", .g1g2EcCodewordsCount=" << static_cast<size_t>(g1g2_ec_codewords_count)
           << ", .g1DataCodewordsCount=" << static_cast<size_t>(g1_data_codewords_count)
           << ", .dataCodewordsCount=" << data_codewords_count
           << ", .writableDataCodewordsCount=" << writable_data_codewords_count << "}";

        m_polyGenDegrees_.insert(g1g2_ec_codewords_count);

        const uint8_t g1g2_codewords_count =
            g1g2_ec_codewords_count + g1_data_codewords_count + (g2_blocks_count > 0 ? 1 : 0);

        largest_codewords_count = std::max(largest_codewords_count, codewords_count);
        largest_modules_count = std::max(largest_modules_count, modules_count);
        largest_g1g2_codewords_count = std::max(largest_g1g2_codewords_count, g1g2_codewords_count);
        largest_g1g2_ec_codewords_count =
            std::max(largest_g1g2_ec_codewords_count, g1g2_ec_codewords_count);

        if (e != 3)
          ss << ", ";
      }

      ss << "}}";

      if (v != 40)
        ss << ", ";
    }

    ss << "}};\n\n";
    m_fileStream_ << ss.str();

    std::cout << "[DEBUG] jldQrCodeMemoizer -> Largest EC/block: "
              << static_cast<size_t>(largest_g1g2_ec_codewords_count) << ".\n";
    std::cout << "[DEBUG] jldQrCodeMemoizer -> Largest Codewords/block: "
              << static_cast<size_t>(largest_g1g2_codewords_count) << ".\n";
    std::cout << "[DEBUG] jldQrCodeMemoizer -> Largest Codewords/total: " << largest_codewords_count
              << ".\n";
    std::cout << "[DEBUG] jldQrCodeMemoizer -> Largest Modules/total: " << largest_modules_count
              << ".\n";

    if (c_maxPolyGenSize != largest_g1g2_ec_codewords_count) {
      std::cerr << "[ERROR] jldQrCodeMemoizer -> Largest EC/block exceeds c_maxPolyGenerators\n";
      m_isValid_ = false;
      return;
    }

    if (c_maxPolyCoeffsSize != (largest_g1g2_ec_codewords_count + 1)) {
      std::cerr << "[ERROR] jldQrCodeMemoizer -> Largest EC/block+1 exceeds c_maxPolyCoeffsSize\n";
      m_isValid_ = false;
      return;
    }

    if (c_maxPolyDivDataSize != largest_g1g2_codewords_count) {
      std::cerr
          << "[ERROR] jldQrCodeMemoizer -> Largest Codewords/block exceeds c_maxPolyDivDataSize\n";
      m_isValid_ = false;
      return;
    }

    if (c_maxCodewordsCount != largest_codewords_count) {
      std::cerr
          << "[ERROR] jldQrCodeMemoizer -> Largest Codewords/total exceeds c_maxCodewordsCount\n";
      m_isValid_ = false;
      return;
    }

    if (c_maxModulesCount != largest_modules_count) {
      std::cerr << "[ERROR] jldQrCodeMemoizer -> Largest Modules/total exceeds c_maxModulesCount\n";
      m_isValid_ = false;
      return;
    }

    m_encodingInfoGenerated_ = true;
  }

  void generate_poly_generator() {
    if (!m_isValid_)
      return;

    if (!m_encodingInfoGenerated_) {
      std::cerr
          << "[ERROR] jldQrCodeMemoizer -> The polynomial generator table requires the encoding "
             "info table, "
             "generate it first.\n";
      return;
    }

    if (!m_expTableGenerated_) {
      std::cerr << "[ERROR] jldQrCodeMemoizer -> The polynomial generator table requires the "
                   "exponential table, "
                   "generate it first.\n";
      return;
    }

    if (!m_logTableGenerated_) {
      std::cerr << "[ERROR] jldQrCodeMemoizer -> The polynomial generator table requires the "
                   "logarithmic table, "
                   "generate it first.\n";
      return;
    }

    std::stringstream ss;

    ss << "\n// Structured, like:"
          "\n// Generator degree / EC/block -> { coefficients }"
          "\n// 7-1  -> { ... }"
          "\n// 8-1  -> { ... }"
          "\n// 9-1  -> { ... }"
          "\n// ..."
          "\n// Degrees that are never used are empty"
       << "\nstatic inline constexpr std::array<std::array<uint8_t, "
       << static_cast<size_t>(c_maxPolyCoeffsSize) << ">, " << static_cast<size_t>(c_maxPolyGenSize)
       << "> c_polyGenTable{{";

    for (uint8_t i = 0; i < c_maxPolyGenSize; ++i) {
      ss << "{{";

      const auto itr = m_polyGenDegrees_.find(i + 1);
      if (itr != m_polyGenDegrees_.end()) {
        poly_generator(*itr);

        for (size_t j = 0; j < m_polyCoeffs_.size; ++j) {
          ss << static_cast<size_t>(m_polyCoeffs_[j]);
          if (j != m_polyCoeffs_.size - 1)
            ss << ", ";
        }
      }

      ss << "}}";

      if (i != c_maxPolyGenSize - 1)
        ss << ", ";
    }

    ss << "}};\n";
    m_fileStream_ << ss.str();
  }

  void generate_format_info() {
    if (!m_isValid_)
      return;

    constexpr std::array<uint8_t, 15> format_mask{ 1, 0, 1, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0 };

    std::stringstream ss;
    ss << "\n// Structured, like:"
          "\n// M { Mask 0: { format info }, ...1: { ... }, ... Mask 7: { ... } }"
          "\n// L { ... }"
          "\n// H { ... }"
          "\n// Q { ... }"
          "\n// Debug { 0: { Debug Data } }"
          "\n// Mask index: 0-7"
          "\n// Format information: 15 bytes"
       << "\nstatic inline constexpr std::array<std::array<std::array<uint8_t, 15>, 8>, 5> "
          "c_formatInfoTable{{";

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

    // Generate data for error correction level 1-5 (5 as debug level) and mask index 0-7
    for (uint8_t e = 0; e < 5; ++e) {
      ss << "{{";

      // Debug level
      if (e == 4) {
        ss << "{";

        // Just grayscale values to visualize the placement

        BchArray bch_data;
        bch_data[0] = 0;
        bch_data[1] = 18;
        bch_data[2] = 36;
        bch_data[3] = 54;
        bch_data[4] = 73;
        bch_data[5] = 91;
        bch_data[6] = 109;
        bch_data[7] = 128;
        bch_data[8] = 146;
        bch_data[9] = 164;
        bch_data[10] = 182;
        bch_data[11] = 201;
        bch_data[12] = 219;
        bch_data[13] = 237;
        bch_data[14] = 255;
        bch_data.size = 15;

        for (uint8_t i = 0; i < 15; ++i) {
          ss << static_cast<size_t>(bch_data[i]);
          if (i != 14)
            ss << ", ";
        }

        ss << "}\n";
      } else {
        for (uint8_t m = 0; m < 8; ++m) {
          ss << "        {";

          BchArray bch_data;
          bch_data[0] = e >> 1;
          bch_data[1] = e & 1;
          bch_data[2] = m >> 2;
          bch_data[3] = (m >> 1) & 1;
          bch_data[4] = m & 1;
          bch_data.size = 15;

          bch(bch_data, 5);

          for (uint8_t i = 0; i < 15; ++i) {
            ss << ((bch_data[i] ^ format_mask[i]) == 1 ? 0 : 255);
            if (i != 14)
              ss << ", ";
          }

          ss << "}";

          if (m != 7)
            ss << ", ";
        }
      }

      ss << "}}";

      if (e != 4)
        ss << ", ";
    }

    ss << "}};\n";
    m_fileStream_ << ss.str();
  }

  void generate_version_info() {
    if (!m_isValid_)
      return;

    std::stringstream ss;
    ss << "\n// Structured, like:"
          "\n// V1  { Debugging info }"
          "\n// V2  { Empty }"
          "\n// ..."
          "\n// V6  { Empty }"
          "\n// V7  { Version info }"
          "\n// ..."
          "\n// V40 { Version info }\n"
          "\n// Version information: 18 bytes"
       << "\nstatic inline constexpr std::array<std::array<uint8_t, 18>, 40> "
          "c_versionInfoTable{{";

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

    // Generate version info for version 7-40 and debugging info as version 1
    for (uint8_t v = 1; v < 41; ++v) {
      ss << "{";

      // Debugging info
      if (v == 1) {
        // Just grayscale values to visualize the placement

        BchArray bch_data;
        bch_data[0] = 0;
        bch_data[1] = 15;
        bch_data[2] = 30;
        bch_data[3] = 45;
        bch_data[4] = 60;
        bch_data[5] = 75;
        bch_data[6] = 90;
        bch_data[7] = 105;
        bch_data[8] = 120;
        bch_data[9] = 135;
        bch_data[10] = 150;
        bch_data[11] = 165;
        bch_data[12] = 180;
        bch_data[13] = 195;
        bch_data[14] = 210;
        bch_data[15] = 225;
        bch_data[16] = 240;
        bch_data[17] = 255;
        bch_data.size = 18;

        std::ranges::reverse(bch_data);

        for (uint8_t i = 0; i < 18; ++i) {
          ss << static_cast<size_t>(bch_data[i]);
          if (i != 17)
            ss << ", ";
        }
      }

      if (v < 7) {
        ss << "}, ";
        continue;
      }

      BchArray bch_data;
      bch_data[0] = (v >> 5) & 1;
      bch_data[1] = (v >> 4) & 1;
      bch_data[2] = (v >> 3) & 1;
      bch_data[3] = (v >> 2) & 1;
      bch_data[4] = (v >> 1) & 1;
      bch_data[5] = v & 1;
      bch_data.size = 18;

      bch(bch_data, 6);
      std::ranges::reverse(bch_data);

      for (uint8_t i = 0; i < 18; ++i) {
        ss << (bch_data[i] == 1 ? 0 : 255);
        if (i != 18)
          ss << ", ";
      }

      ss << "}";

      if (v != 40)
        ss << ", ";
    }

    ss << "}};\n";
    m_fileStream_ << ss.str();
  }

  [[nodiscard]] auto gf256_mul(uint8_t a, uint8_t b) const -> uint8_t {
    return m_expTable_[(m_logTable_[a] + m_logTable_[b]) % 255];
  }

  void poly_mul(uint8_t factor) {
    util_assert(factor > 0, "[poly_mul] factor must be non-zero");
    util_assert(m_polyCoeffs_.size <= c_maxPolyCoeffsSize,
                "[poly_mul] this iteration exceeds c_maxPolyCoeffs");

    // TODO(jld-wk): Explain

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

  void poly_generator(uint8_t degree) {
    util_assert(degree > 0, "[poly_generator] degree must be non-zero");

    m_polyCoeffs_[0] = 1;
    m_polyCoeffs_.size = 1;
    for (uint8_t i = 0; i < degree; ++i) poly_mul(m_expTable_[i]);
  }

  void bch(BchArray& out, uint32_t data_size) {
    util_assert(out.size >= data_size, "[bch] out.size must be greater than data_size");
    util_assert(out.size <= m_polyDivData_.data.size(), "[bch] out.size exceeds m_polyDivData_");
    util_assert((data_size + m_polyCoeffs_.size - 1) == out.size,
                "[bch] data_size with given m_polyCoeffs_ must be equal to out.size");

    // TODO(jld-wk): Explain

    for (size_t i = 0; i < out.size; ++i) m_polyDivData_[i] = out[i];
    m_polyDivData_.size = out.size;

    for (uint32_t i = 0; i < data_size; ++i) {
      uint8_t start{ m_polyDivData_[i] };
      if (start != 0)
        for (size_t g = 0; g < m_polyCoeffs_.size; ++g) m_polyDivData_[i + g] ^= m_polyCoeffs_[g];
    }
    for (size_t i = data_size; i < out.size; ++i) out[i] = m_polyDivData_[i];
  }

  constexpr void util_assert(bool condition, const char* message) {
    if consteval {
      assert(condition);
    }

    if (!condition) {
      std::cerr << "[ASSERT] jldQrCodeMemoizer -> " << message << ".\n";
      std::terminate();
    }
  }

 private:
  std::fstream m_fileStream_;
  bool         m_isValid_ = false;

  bool m_logTableGenerated_ = false;
  bool m_expTableGenerated_ = false;
  bool m_encodingInfoGenerated_ = false;

  std::unordered_set<uint8_t> m_polyGenDegrees_;

  std::array<uint8_t, 256> m_logTable_{};
  std::array<uint8_t, 256> m_expTable_{ 1 };

  ByteArray<c_maxPolyCoeffsSize>  m_polyCoeffs_;
  ByteArray<c_maxPolyDivDataSize> m_polyDivData_;
};

}  // namespace jld

#endif