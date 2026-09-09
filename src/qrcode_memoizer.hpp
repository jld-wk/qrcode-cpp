#ifndef QRCODE_MEMOIZER
#define QRCODE_MEMOIZER

#include <array>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <unordered_set>

#include "qrcode_arrays.hpp"

/*

This struct is explained in the corresponding 'generate_encoding_info' function below

*/
struct QrCodeEncodingInfo {
  size_t total_modules = 0;
  // QRCodes are squared, so width = height as well
  size_t modules_width = 0;
  size_t data_size_bit_len = 0;

  size_t ec_codewords = 0;
  size_t data_codewords = 0;
  // 'total_codewords' is just 'data_codewords + ec_codewords'

  size_t g1_blocks = 0;
  size_t g2_blocks = 0;
  // 'total_blocks' is just 'g1_blocks + g2_blocks'

  size_t ec_codewords_per_block = 0;
  size_t g1_data_codewords_per_block = 0;
  // 'g2_data_codewords_per_block' is just 'g1_data_codewords_per_block + 1'

  size_t writable_codewords = 0;
};

/*

Note: The memoizer is only designed for byte encoding, therefore everything generated may not work
for other encoding methods

*/
class QrCodeMemoizer {
 public:
  QrCodeMemoizer(std::string filepath)
      : m_fileStream_{ filepath, std::ios::out }
      , m_isValid_{ m_fileStream_.is_open() } {
    if (!m_fileStream_.is_open()) {
      std::cerr << "QrCodeMemoizer -> Failed to open file: " << filepath << ".\n";
      return;
    }

    const char* header =
        "/*\n\n    Memoized QRCode info for https://www.github.com/jld.wk/qrcode-c++\n    Licensed "
        "under "
        "[TODO]. \n    Generated using "
        "https://www.github.com/jld.wk/qrcode-c++/blob/main/qrcode_memoizer.hpp.\n    Do not "
        "edit!\n\n*/\n\n#include <cstddef>\n#include <cstdint>\n#include <array>\n";
    m_fileStream_ << header
                  << "\n// Largest 'EC/block + 1' -> Do not edit!\nstatic constexpr size_t "
                     "c_maxPolyCoeffs = "
                  << c_maxPolyCoeffs_
                  << ";\n// Largest 'EC/block' -> Do not edit!\nstatic constexpr size_t "
                     "c_maxPolyGenerators = "
                  << c_maxPolyGenerators
                  << ";\n// Largest 'EC/block + G2 data/block' -> Do not "
                     "edit!\nstatic constexpr size_t c_maxPolyDivData = "
                  << c_maxPolyDivData << ";";
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
    const std::array<std::array<std::array<size_t, 4>, 4>, 40> qr_block_table{ {
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

    std::array<std::array<QrCodeEncodingInfo, 4>, 40> qr_encoding_info_table{};

    // Generate encoding info for version 1-40 and error correction level 1-4
    for (size_t v = 1; v < 41; ++v) {
      for (size_t e = 0; e < 4; ++e) {
        // Count of alignment + finder patterns
        const size_t total_patterns{ static_cast<size_t>(std::floor(static_cast<double>(v) / 7) +
                                                         2) };
        // QRCode size: 'modules_width' x 'modules_width'
        const size_t modules_width = (v * 4) + 17;
        // The amount of bits to use to encode the size of the data to encode
        const size_t data_size_bit_len = v >= 10 ? 16 : 8;

        const size_t total_modules{ modules_width * modules_width };

        // The amount of modules each reserved area requires: \/
        constexpr size_t finder_pattern_modules = 3 * 8 * 8;
        const size_t     alignment_pattern_modules{
          v == 1 ? 0 : (total_patterns * total_patterns - 3) * 5 * 5
        };
        const size_t timing_pattern_modules{ 2 * (4 * static_cast<size_t>(v) + 1) };
        const size_t intersection_modules{ total_patterns == 1 ? 0 : 2 * (total_patterns - 2) * 5 };

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

        const size_t data_modules = total_modules - finder_pattern_modules -
                                    alignment_pattern_modules - timing_pattern_modules +
                                    intersection_modules - (1) - format_information_modules -
                                    (v >= 7 ? version_information_modules : 0);

        const size_t                version_idx = v - 1;
        const std::array<size_t, 4> table_entry = qr_block_table[version_idx][e];

        const size_t ec_codewords_per_block = table_entry[0];
        const size_t g1_blocks = table_entry[1];
        const size_t g1_data_codewords_per_block = table_entry[2];
        const size_t g2_blocks = table_entry[3];

        const size_t total_blocks = g1_blocks + g2_blocks;
        const size_t ec_codewords = ec_codewords_per_block * total_blocks;

        constexpr size_t byte_in_bits = 8;

        const size_t total_codewords = data_modules / byte_in_bits;
        const size_t data_codewords = total_codewords - ec_codewords;

        // 2 or 3 bytes (depending on the version) are reserved for the encoding mode (4
        // bits -> rounded 1 byte) and data size (2 bytes for versions 10 and above and 1 byte for
        // any version below 10)
        const size_t writable_codewords = data_codewords - 1 - (data_size_bit_len / byte_in_bits);

        qr_encoding_info_table[version_idx][e] =
            QrCodeEncodingInfo{ .total_modules = total_modules,
                                .modules_width = modules_width,
                                .data_size_bit_len = data_size_bit_len,
                                .ec_codewords = ec_codewords,
                                .data_codewords = data_codewords,
                                .g1_blocks = g1_blocks,
                                .g2_blocks = g2_blocks,
                                .ec_codewords_per_block = ec_codewords_per_block,
                                .g1_data_codewords_per_block = g1_data_codewords_per_block,
                                .writable_codewords = writable_codewords };
      }
    }

    std::stringstream ss;
    ss << "\n"
          "/*\n"
          "  Structured, like:\n"
          "  V1-1   M { Total modules,\n"
          "             Modules Width,\n"
          "             Data size bit length,\n"
          "             EC codewords,\n"
          "             Data codewords,\n"
          "             G1 blocks,\n"
          "             G2 blocks,\n"
          "             EC/block,\n"
          "             G1 data/block,\n"
          "             Writable codewords\n"
          "           } L { ... } H { ... } Q { ... }\n"
          "  V2-1   ... ->\n"
          "  V3-1   ... ->\n"
          "  ||\n"
          "  \\/\n\n"
          "  For more information, check out\n"
          "  https://www.github.com/jld.wk/qrcode-c++/blob/main/qrcode_memoizer.hpp\n"
          "*/\n";
    ss << "static inline constexpr std::array<std::array<std::array<size_t, 10>, 4>, 40> "
          "c_qrEncodingInfoTable{{\n";

    size_t largest_ec_codewords_per_block = 0;
    size_t largest_total_codewords_per_block = 0;

    for (size_t version = 0; version < 40; ++version) {
      ss << "    {{";

      for (size_t ec = 0; ec < 4; ++ec) {
        const QrCodeEncodingInfo& info = qr_encoding_info_table[version][ec];

        ss << "{{" << info.total_modules << ", " << info.modules_width << ", "
           << info.data_size_bit_len << ", " << info.ec_codewords << ", " << info.data_codewords
           << ", " << info.g1_blocks << ", " << info.g2_blocks << ", "
           << info.ec_codewords_per_block << ", " << info.g1_data_codewords_per_block << ", "
           << info.writable_codewords << "}}";

        m_ecCodewordsPerBlock_.insert(info.ec_codewords_per_block);

        size_t total_codewords = info.ec_codewords_per_block + info.g1_data_codewords_per_block +
                                 (info.g2_blocks > 0 ? 1 : 0);

        if (largest_ec_codewords_per_block < info.ec_codewords_per_block)
          largest_ec_codewords_per_block = info.ec_codewords_per_block;

        if (largest_total_codewords_per_block < total_codewords)
          largest_total_codewords_per_block = total_codewords;

        if (ec != 3)
          ss << ", ";
      }

      ss << "}}";

      if (version != 39)
        ss << ",";

      ss << "\n";
    }

    ss << "}};\n\n";
    m_fileStream_ << ss.str();

    std::cout << "[Debug] QrCodeMemoizer -> Largest EC/block: " << largest_ec_codewords_per_block
              << ".\n";
    std::cout << "[Debug] QrCodeMemoizer -> Largest Codewords/block: "
              << largest_total_codewords_per_block << ".\n";

    // Just making sure...
    if (c_maxPolyCoeffs_ != (largest_ec_codewords_per_block + 1)) {
      std::cerr << "QrCodeMemoizer -> Largest EC/block+1 doesn't match the size of the polynomial "
                   "coefficients array.\n";
      m_isValid_ = false;
    } else if (c_maxPolyGenerators != largest_ec_codewords_per_block) {
      std::cerr << "QrCodeMemoizer -> Largest EC/block doesn't match the size of the polynomial "
                   "generators array.\n";
      m_isValid_ = false;
    } else if (c_maxPolyDivData != largest_total_codewords_per_block) {
      std::cerr
          << "QrCodeMemoizer -> Largest Codewords/block doesn't match the size of the polynomial "
             "division input array.\n";
      m_isValid_ = false;
    } else
      m_encodingInfoGenerated_ = true;
  }

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
      ss << m_expTable_[i] + 0;
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
      std::cerr << "QrCodeMemoizer -> The logarithmic table requires the exponential table, "
                   "generate it first.\n";
      return;
    }

    for (size_t i = 0; i < m_logTable_.size(); ++i)
      m_logTable_[m_expTable_[i]] = static_cast<uint8_t>(i);

    std::stringstream ss;
    ss << "\nstatic inline constexpr std::array<uint8_t, 256> "
          "c_logTable{\n    ";

    for (size_t i = 0; i < m_logTable_.size(); ++i) {
      ss << m_logTable_[i] + 0;
      if (i != 255)
        ss << ", ";
    }

    ss << "\n};\n";
    m_fileStream_ << ss.str();

    m_logTableGenerated_ = true;
  }

  void generate_polynomial_generator() {
    if (!m_isValid_)
      return;

    if (!m_encodingInfoGenerated_) {
      std::cerr
          << "QrCodeMemoizer -> The polynomial generator table requires the encoding info table, "
             "generate it first.\n";
      return;
    }

    if (!m_expTableGenerated_) {
      std::cerr
          << "QrCodeMemoizer -> The polynomial generator table requires the exponential table, "
             "generate it first.\n";
      return;
    }

    if (!m_logTableGenerated_) {
      std::cerr
          << "QrCodeMemoizer -> The polynomial generator table requires the logarithmic table, "
             "generate it first.\n";
      return;
    }

    std::stringstream ss;

    ss << "\n"
          "/*\n"
          "  Structured, like:\n"
          "  Generator degree / EC/block -> { coefficients }\n"
          "  7-1  -> { ... }\n"
          "  8-1  -> { ... }\n"
          "  9-1  -> { ... }\n"
          "  ...\n"
          "  Degrees that are never used are empty, as you might have already noticed.\n"
          "*/\n";

    ss << "static inline constexpr std::array<std::array<uint8_t, " << c_maxPolyCoeffs_ << ">, "
       << c_maxPolyGenerators
       << "> "
          "c_polynomialGeneratorTable{{\n";

    for (size_t i = 0; i < c_maxPolyGenerators; ++i) {
      ss << "    {{";

      const auto itr = m_ecCodewordsPerBlock_.find(i + 1);
      if (itr != m_ecCodewordsPerBlock_.end()) {
        polynomial_generator(*itr);

        for (size_t j = 0; j < m_polyCoeffsSize_; ++j) {
          ss << static_cast<size_t>(m_polyCoeffs_[j]);
          if (j + 1 != m_polyCoeffsSize_)
            ss << ", ";
        }
      }

      ss << "}}";

      if (i + 1 != c_maxPolyGenerators)
        ss << ",";

      ss << "\n";
    }

    ss << "}};\n";
    m_fileStream_ << ss.str();
  }

  void generate_format_information() {
    if (!m_isValid_)
      return;

    const std::array<uint8_t, 15> format_mask{ 1, 0, 1, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0 };

    std::stringstream ss;

    ss << "\n"
          "/*\n"
          "  Structured, like:\n"
          "  M { Mask 0: { format information }, ...1: { ... }, ... Mask 7: { ... } }\n"
          "  L { ... }\n"
          "  H { ... }\n"
          "  Q { ... }\n"
          "  Debug { 0: { Debug Data } }\n\n"
          "  Mask index: 0-7\n"
          "  Format information: 15 bytes\n\n"
          "  For more information, check out\n"
          "  https://www.github.com/jld.wk/qrcode-c++/blob/main/qrcode_memoizer.hpp\n"
          "*/\n";

    ss << "static inline constexpr std::array<std::array<std::array<uint8_t, 15>, 8>, 5> "
          "c_formatInformationTable{{\n";

    // Generate data for error correction level 1-5 (5 as debug level) and mask index 0-7
    for (size_t e = 0; e < 5; ++e) {
      ss << "    {{\n";

      // Debug level
      if (e == 4) {
        ss << "        {";

        // Just grayscale values to visualize the placement
        BchArray debug_data;
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

        for (size_t i = 0; i < debug_data.size; ++i) {
          ss << static_cast<size_t>(debug_data[i]);
          if (i + 1 != debug_data.size)
            ss << ", ";
        }

        ss << "}\n";
      } else {
        for (size_t m = 0; m < 8; ++m) {
          ss << "        {";

          BchArray format_info;
          format_info[0] = e >> 1;
          format_info[1] = e & 1;
          format_info[2] = m >> 2;
          format_info[3] = (m >> 1) & 1;
          format_info[4] = m & 1;
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
          m_polyCoeffsSize_ = 11;

          bch(format_info, 5);

          for (size_t i = 0; i < format_info.size; ++i) format_info[i] ^= format_mask[i];

          for (size_t i = 0; i < format_info.size; ++i) {
            ss << (format_info[i] == 1 ? 0 : 255);
            if (i + 1 != format_info.size)
              ss << ", ";
          }

          ss << "}";

          if (m + 1 != 8)
            ss << ",";

          ss << "\n";
        }
      }

      ss << "    }}";

      if (e + 1 != 5)
        ss << ",";

      ss << "\n";
    }

    ss << "}};\n";
    m_fileStream_ << ss.str();
  }

  void generate_version_information() {
    if (!m_isValid_)
      return;

    std::stringstream ss;

    ss << "\n"
          "/*\n"
          "  Structured, like:\n"
          "  V1  { Debugging info }\n"
          "  V2  { Empty }\n"
          "  ...\n"
          "  V6  { Empty }\n"
          "  V7  { Version information }\n"
          "  ...\n"
          "  V40 { Version information }\n\n"
          "  Version information: 18 bytes\n"
          "  ||\n"
          "  \\/\n\n"
          "  For more information, check out\n"
          "  https://www.github.com/jld.wk/qrcode-c++/blob/main/qrcode_memoizer.hpp\n"
          "*/\n";

    ss << "static inline constexpr std::array<std::array<uint8_t, 18>, 40> "
          "c_versionInformationTable{{\n";

    // Generate version info for version 7-40 and debugging info as version 1
    for (size_t v = 1; v < 41; ++v) {
      ss << "    {";

      // Debugging info
      if (v == 1) {
        // Just grayscale values to visualize the placement
        BchArray debug_data;
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

        std::reverse(debug_data.begin(), debug_data.end());

        for (size_t i = 0; i < 18; ++i) {
          ss << static_cast<size_t>(debug_data[i]);
          if (i != 17)
            ss << ", ";
        }
      }

      if (v < 7) {
        ss << "},\n";
        continue;
      }

      BchArray version_info;
      version_info[0] = (v >> 5) & 1;
      version_info[1] = (v >> 4) & 1;
      version_info[2] = (v >> 3) & 1;
      version_info[3] = (v >> 2) & 1;
      version_info[4] = (v >> 1) & 1;
      version_info[5] = v & 1;
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
      m_polyCoeffsSize_ = 13;

      bch(version_info, 6);
      std::reverse(version_info.begin(), version_info.end());

      for (size_t i = 0; i < 18; ++i) {
        ss << (version_info[i] == 1 ? 0 : 255);
        if (i != 17)
          ss << ", ";
      }

      ss << "}";

      if (v != 40)
        ss << ",";

      ss << "\n";
    }

    ss << "}};\n";
    m_fileStream_ << ss.str();
  }

 private:
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

  void polynomial_generator(size_t degree) {
    assert(degree > 0 && degree <= 256);
    m_polyCoeffs_[0] = 1;
    m_polyCoeffsSize_ = 1;
    for (size_t i = 0; i < degree; ++i) polynomial_multiplication(m_expTable_[i]);
  }

  void bch(BchArray& out, size_t data_size) {
    assert(out.size >= data_size);

    for (size_t i = 0; i < out.size; ++i) m_polyDivData_[i] = out[i];
    m_polyDivData_.size = out.size;

    for (size_t i = 0; i < data_size; ++i) {
      uint8_t start{ m_polyDivData_[i] };
      if (start != 0)
        for (size_t g = 0; g < m_polyCoeffsSize_; ++g) m_polyDivData_[i + g] ^= m_polyCoeffs_[g];
    }
    for (size_t i = data_size; i < out.size; ++i) out[i] = m_polyDivData_[i];
  }

 private:
  // Largest 'EC/block + 1' -> Do not edit!
  static constexpr size_t c_maxPolyCoeffs_ = 31;
  // Largest 'EC/block' -> Do not edit!
  static constexpr size_t c_maxPolyGenerators = 30;
  // Largest 'EC/block + G2 data/block' -> Do not edit!
  static constexpr size_t c_maxPolyDivData = 153;

  std::fstream m_fileStream_;
  bool         m_isValid_ = false;

  bool m_logTableGenerated_ = false;
  bool m_expTableGenerated_ = false;
  bool m_encodingInfoGenerated_ = false;

  std::unordered_set<size_t> m_ecCodewordsPerBlock_;

  std::array<uint8_t, 256> m_logTable_{};
  std::array<uint8_t, 256> m_expTable_{ 1 };

  std::array<uint8_t, c_maxPolyCoeffs_> m_polyCoeffs_;
  size_t                                m_polyCoeffsSize_ = 0;
  StackArray<uint8_t, c_maxPolyDivData> m_polyDivData_;
};

#endif