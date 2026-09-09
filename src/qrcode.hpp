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
#include <type_traits>
#include <vector>

#include "qrcode_arrays.hpp"
#include "qrcode_memoized.hpp"
#include "qrcode_pixels.hpp"

class CodewordBitReader {
 public:
  CodewordBitReader(const CodewordArray& codewords, bool alternate_bits)
      : m_codewords_{ codewords }
      , m_alternateBits_{ alternate_bits } {}
  ~CodewordBitReader() = default;

  CodewordBitReader(const CodewordBitReader&) = delete;
  CodewordBitReader(CodewordBitReader&&) = delete;
  CodewordBitReader& operator=(const CodewordBitReader&) = delete;
  CodewordBitReader& operator=(CodewordBitReader&&) = delete;

  uint8_t next() {
    size_t  idx = m_bitIdx_--;
    uint8_t bit = m_codewordBits_[idx];

    if (idx == 0) {
      ++m_wordIdx_;
      m_bitIdx_ = 7;
      if (m_wordIdx_ < m_codewords_.size)
        m_codewordBits_ = m_codewords_[m_wordIdx_];
    }

    if (m_alternateBits_) {
      m_alternate_ = !m_alternate_;
      return m_alternate_ ? 0 : 1;
    }
    return bit == 1 ? 0 : 255;
  }

  bool in_bounds() const {
    return m_wordIdx_ != m_codewords_.size;
  }

 private:
  bool m_alternate_ = false;
  bool m_alternateBits_ = false;

  size_t m_bitIdx_ = 7;
  size_t m_wordIdx_ = 0;

  const CodewordArray& m_codewords_;
  std::bitset<8>       m_codewordBits_{ m_codewords_[0] };
};

class QrCodeDebugFlag {
 public:
  static constexpr uint32_t None = 0;
  static constexpr uint32_t DebugFormatInfo = 1;
  static constexpr uint32_t DebugVersionInfo = 1 << 1;
  static constexpr uint32_t DebugTimingPatterns = 1 << 2;
  static constexpr uint32_t PrintInBinary = 1 << 3;
  static constexpr uint32_t PrintRawCodewords = 1 << 4;
  static constexpr uint32_t PrintInterleavedCodewords = 1 << 5;
  static constexpr uint32_t PrintErrorCorrectionData = 1 << 6;
  static constexpr uint32_t DisableMasking = 1 << 7;
  static constexpr uint32_t UseCustomMaskIdx = 1 << 8;
  static constexpr uint32_t DebugBitPlacing = 1 << 9;
  static constexpr uint32_t DisableBitPlacing = 1 << 10;

  const uint32_t mask = None;

  constexpr QrCodeDebugFlag(uint32_t mask)
      : mask{ mask } {}

  constexpr operator uint32_t() {
    return mask;
  }

  constexpr QrCodeDebugFlag operator|(uint32_t b) const {
    return QrCodeDebugFlag{ mask | b };
  }

  constexpr QrCodeDebugFlag operator&(uint32_t b) const {
    return QrCodeDebugFlag{ mask & b };
  }
};

enum class Ecc : uint8_t { M = 0, L = 1, H = 2, Q = 3 };

struct QrCodeInfo {
  Ecc     ecc = Ecc::M;
  uint8_t version = 1;
};

struct QrCodeGenerationInfo {
  uint8_t something = 0;
};

class QrCodeGenerator {
 public:
  QrCodeGenerator() {
    for (uint8_t& e : m_rawModules_) e = 255;
    for (uint8_t& e : m_maskedModules_) e = 255;
    for (size_t i = 0; i < m_reservedModules_.size(); ++i) m_reservedModules_[i] = 0;
  }

  template <QrCodeInfo Info>
  constexpr QrCodeGenerationInfo gen_info() {
    static_assert(Info.version > 0 && Info.version <= 40);
    return QrCodeGenerationInfo{ .something = 10 };
  }

  // TODO: Could be constexpr as well? Well, at least retrieving the data from all the tables
  constexpr void generate(const std::vector<uint8_t>& data, QrCodeInfo gen_info,
                          QrCodeDebugFlag debug_flags = QrCodeDebugFlag::None) {
    assert(gen_info.version != 0 && gen_info.version <= 40);

    const size_t                 version_idx = gen_info.version - 1;
    const std::array<size_t, 10> info =
        c_qrEncodingInfoTable[version_idx][static_cast<size_t>(gen_info.ecc)];

    const size_t total_modules = info[0];
    const size_t modules_width = info[1];
    const size_t data_size_bit_length = info[2];
    const size_t ec_codewords = info[3];
    const size_t data_codewords = info[4];
    const size_t g1_blocks = info[5];
    const size_t g2_blocks = info[6];
    const size_t ec_codewords_per_block = info[7];
    const size_t g1_codewords_per_block = info[8];
    const size_t writable_codewords = info[9];

    const size_t total_blocks = g1_blocks + g2_blocks;
    const size_t total_codewords = ec_codewords + data_codewords;

    const size_t g2_codewords_per_block = g1_codewords_per_block + 1;

    if (data.size() > writable_codewords) {
      // TODO: Proper error handling
      std::cerr << "Too many bytes of data to encode!\n";
      return;
    }

    fill_codewords(data, data_codewords, total_codewords, data_size_bit_length);

    if (debug_flags & QrCodeDebugFlag::PrintRawCodewords) {
      if (debug_flags & QrCodeDebugFlag::PrintInBinary) {
        std::cout << "Codewords (Binary): [ ";
        for (size_t i = 0; i < data_codewords; ++i)
          std::cout << std::bitset<8>(m_rawCodewords_[i]) << " - ";
        std::cout << " ]\n\n";
      } else {
        std::cout << "Codewords (Decimal): [ ";
        for (size_t i = 0; i < data_codewords; ++i)
          std::cout << static_cast<size_t>(m_rawCodewords_[i]) << " - ";
        std::cout << " ]\n\n";
      }
    }

    interleave_codewords(g1_blocks, g1_codewords_per_block, g2_blocks, g2_codewords_per_block,
                         total_blocks);

    if (debug_flags & QrCodeDebugFlag::PrintInterleavedCodewords) {
      if (debug_flags & QrCodeDebugFlag::PrintInBinary) {
        std::cout << "Interleaved Codewords (Binary): [ ";
        for (size_t i = 0; i < data_codewords; ++i)
          std::cout << std::bitset<8>(m_interleavedCodewords_[i]) << " - ";
        std::cout << " ]\n\n";
      } else {
        std::cout << "Interleaved Codewords (Decimal): [ ";
        for (size_t i = 0; i < data_codewords; ++i)
          std::cout << static_cast<size_t>(m_interleavedCodewords_[i]) << " - ";
        std::cout << " ]\n\n";
      }
    }

    m_polyCoeffs_ = c_polynomialGeneratorTable[ec_codewords_per_block - 1];
    m_polyCoeffsSize_ = ec_codewords_per_block + 1;

    generate_ec_data(g1_blocks, g1_codewords_per_block, g2_blocks, g2_codewords_per_block,
                     total_blocks, ec_codewords_per_block, data_codewords, ec_codewords);

    if (debug_flags & QrCodeDebugFlag::PrintErrorCorrectionData) {
      if (debug_flags & QrCodeDebugFlag::PrintInBinary) {
        std::cout << "Error-Correction Data (Binary): [ ";
        for (size_t i = data_codewords; i < m_interleavedCodewords_.size; ++i)
          std::cout << std::bitset<8>(m_interleavedCodewords_[i]) << " - ";
        std::cout << " ]\n\n";
      } else {
        std::cout << "Error-Correction Data (Decimal): [ ";
        for (size_t i = data_codewords; i < m_interleavedCodewords_.size; ++i)
          std::cout << static_cast<size_t>(m_interleavedCodewords_[i]) << " - ";
        std::cout << " ]\n\n";
      }
    }

    place_reserved_modules(gen_info.version, modules_width);

    /*TODO: As debug flag
    for (size_t i = 0; i < m_reservedModules_.size(); ++i) {
      m_rawModules_[i] = m_reservedModules_[i] == 1 ? 0 : 255;
    }
    stbi_write_jpg("out.jpg", modules_width, modules_width, 1, m_rawModules_.data(), 100);
*/

    if (!(debug_flags & QrCodeDebugFlag::DisableBitPlacing)) {
      place_interleaved_codewords(modules_width, total_modules,
                                  debug_flags & QrCodeDebugFlag::DebugBitPlacing);
    }

    if (gen_info.version >= 7)
      place_version_info_modules(version_idx, modules_width,
                                 debug_flags & QrCodeDebugFlag::DebugVersionInfo);

    if (!(debug_flags & QrCodeDebugFlag::DisableMasking)) {
      if (debug_flags & QrCodeDebugFlag::UseCustomMaskIdx) {
        // TODO: static assert?
        constexpr uint8_t custom_mask_idx = 0;  // TODO
        assert(custom_mask_idx >= 0 && custom_mask_idx < 8);

        mask_data_pixels<true>(custom_mask_idx, modules_width, total_modules);
        place_format_info_modules<true>(custom_mask_idx, gen_info.ecc, modules_width,
                                        debug_flags & QrCodeDebugFlag::DebugFormatInfo);
      } else {
        apply_best_mask(modules_width, total_modules, gen_info.ecc,
                        debug_flags & QrCodeDebugFlag::DebugFormatInfo);
      }
    }

    // TODO: add a filepath to the generation info
    stbi_write_jpg("out.jpg", modules_width, modules_width, 1, m_rawModules_.data(), 100);
  }

 private:
  void fill_codewords(const std::vector<uint8_t>& data, const size_t data_codewords,
                      const size_t total_codewords, const size_t bit_len) {
    assert(data.size() <= data_codewords);
    assert(bit_len == 8 || bit_len == 16);
    assert(total_codewords > data_codewords);
    assert(m_rawCodewords_.data.size() >= total_codewords);

    m_rawCodewords_.size = total_codewords;

    // Byte encoding mode -> 0100
    uint8_t       start{ 0b01000000 };
    const uint8_t low = static_cast<uint8_t>(data.size());
    const uint8_t high = bit_len == 8 ? low : static_cast<uint8_t>(data.size() >> 8);

    // Data size encoding
    start |= high >> 4;
    m_rawCodewords_[0] = start;
    m_rawCodewords_[1] = high << 4;

    size_t codeword_idx{ 1 };
    if (bit_len == 16) {
      codeword_idx = 2;
      m_rawCodewords_[1] |= low >> 4;
      m_rawCodewords_[2] = low << 4;
    }

    // Data encoding
    for (uint8_t b : data) {
      m_rawCodewords_[codeword_idx++] |= b >> 4;
      assert(codeword_idx < data_codewords);
      m_rawCodewords_[codeword_idx] = b << 4;
    }

    // 4 zero bits as termination
    m_rawCodewords_[codeword_idx++] &= 0b11110000;

    // Alternating 11101100 and 00010001 for any unused codeword
    bool alternate{ false };
    while (codeword_idx < data_codewords) {
      m_rawCodewords_[codeword_idx++] = alternate ? 0b00010001 : 0b11101100;
      alternate = !alternate;
    }
  }

  void interleave_codewords(const size_t g1_blocks, const size_t g1_codewords_per_block,
                            const size_t g2_blocks, const size_t g2_codewords_per_block,
                            const size_t total_blocks) {
    assert(g1_blocks > 0);
    assert(g2_codewords_per_block == (g1_codewords_per_block + 1));
    assert((g1_blocks + g2_blocks) == total_blocks);

    m_interleavedCodewords_.size = m_rawCodewords_.size;

    // 'm_orderedCodewords_' just equals 'm_codewords_', so only copy it
    if (g1_blocks == 1 && g2_blocks == 0) {
      for (size_t i = 0; i < m_rawCodewords_.size; ++i)
        m_interleavedCodewords_[i] = m_rawCodewords_[i];
      return;
    }

    /*
      Interleaves the codewords.

      Suppose we have this sequence: codewords = { 0, 1, 2, 3, 4, 5, 6, 7 }
      and 'g1_codewords_per_block' is 4 and 'g1_blocks' is 2
      ... then the interleaved codewords would be { 0, 4, 1, 5, 2, 6, 3, 7 }
    */
    size_t interleaved_codewords_idx = 0;
    for (size_t i = 0; i < g2_codewords_per_block; ++i) {
      size_t raw_codeword_idx = 0;
      for (size_t b = 0; b < total_blocks; ++b) {
        const bool g1 = b < g1_blocks;
        // Since 'g2_codewords_per_block' is always 1 larger than 'g1_codewords_per_block' and
        // each group 1 block therfore has one codeword less, we have to skip every group 1 block
        if (g1 && i == g1_codewords_per_block) {
          raw_codeword_idx = g1_blocks * g1_codewords_per_block;
          b = g1_blocks - 1;
          continue;
        }

        m_interleavedCodewords_[interleaved_codewords_idx++] =
            m_rawCodewords_[raw_codeword_idx + i];
        raw_codeword_idx += g1 ? g1_codewords_per_block : g2_codewords_per_block;
      }
    }

    /*
        if (g2_blocks == 0)
          return;

          Did you notice whats missing?
          We only iterated through the count of 'g1_codewords_per_block', but since the
          'g2_codewords_per_block' is always 1 larger, that would mean that we missed the last
       codeword of every

    size_t codeword_idx = (g1_blocks + 1) * g1_codewords_per_block;
    for (size_t b = 0; b < g2_blocks; ++b) {
      m_orderedCodewords_[ordered_codewords_idx++] = m_codewords_[codeword_idx];
      codeword_idx += g2_codewords_per_block;
    }
    */
  }

  void generate_ec_data(const size_t g1_blocks, const size_t g1_codewords_per_block,
                        const size_t g2_blocks, const size_t g2_codewords_per_block,
                        const size_t total_blocks, const size_t ec_codewords_per_block,
                        const size_t data_codewords, const size_t ec_codewords) {
    assert(g1_blocks > 0);
    assert(g2_codewords_per_block == (g1_codewords_per_block + 1));
    assert((g1_blocks + g2_blocks) == total_blocks);
    assert(ec_codewords == (ec_codewords_per_block * total_blocks));
    assert((g1_codewords_per_block * g1_blocks + g2_codewords_per_block * g2_blocks) ==
           data_codewords);

    size_t g1_total_codewords_per_block = g1_codewords_per_block + ec_codewords_per_block;
    size_t g2_total_codewords_per_block = g2_codewords_per_block + ec_codewords_per_block;

    /*
      Generates and interleaves the error correction data.

      Suppose we have this sequence: codewords = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 }
      and 'g1_codewords_per_block' is 4 and 'g1_blocks' is 3 and 'ec_codewords_per_block' is 6

      generate the error correction data for each block (dummy-data):
      ... block 1 = {  0,  1,  2,  3,  4,  5 }
      ... block 2 = {  6,  7,  8,  9, 10, 11 }
      ... block 3 = { 12, 13, 14, 15, 16, 17 }

      and now interleave it ...
      ... ec_data = { 0, 6, 12, 1, 7, 13, 2, 8, 14, 3, 9, 15, 4, 10, 16, 5, 11, 17 }
    */

    size_t offset = 0;
    for (size_t i = 0; i < total_blocks; ++i) {
      const bool   g1 = i < g1_blocks;
      const size_t data_codewords_per_block = g1 ? g1_codewords_per_block : g2_codewords_per_block;
      const size_t total_codewords_per_block =
          g1 ? g1_total_codewords_per_block : g2_total_codewords_per_block;

      set_poly_div_data(data_codewords_per_block, total_codewords_per_block, offset);
      poly_div(data_codewords_per_block, total_codewords_per_block);

      for (size_t c = 0; c < ec_codewords_per_block; ++c) {
        // Directly store the error correction data into the interleaved codewords array
        m_interleavedCodewords_[(c * total_blocks + i) + data_codewords] =
            m_polyDivData_[c + data_codewords_per_block];
      }

      offset += data_codewords_per_block;
    }
  }

  void place_reserved_modules(const size_t version, const size_t modules_width) {
    // TODO: Memoize, probably? Yeah, I don't like this.

    static constexpr std::array<std::array<std::array<uint8_t, 8>, 8>, 3> fp_modules_to_fill{
      { { {
            { 0, 0, 0, 0, 0, 0, 0, 255 },
            { 0, 255, 255, 255, 255, 255, 0, 255 },
            { 0, 255, 0, 0, 0, 255, 0, 255 },
            { 0, 255, 0, 0, 0, 255, 0, 255 },
            { 0, 255, 0, 0, 0, 255, 0, 255 },
            { 0, 255, 255, 255, 255, 255, 0, 255 },
            { 0, 0, 0, 0, 0, 0, 0, 255 },
            { 255, 255, 255, 255, 255, 255, 255, 255 },
        } },
        { {
            { 255, 0, 0, 0, 0, 0, 0, 0 },
            { 255, 0, 255, 255, 255, 255, 255, 0 },
            { 255, 0, 255, 0, 0, 0, 255, 0 },
            { 255, 0, 255, 0, 0, 0, 255, 0 },
            { 255, 0, 255, 0, 0, 0, 255, 0 },
            { 255, 0, 255, 255, 255, 255, 255, 0 },
            { 255, 0, 0, 0, 0, 0, 0, 0 },
            { 255, 255, 255, 255, 255, 255, 255, 255 },
        } },
        { {
            { 255, 255, 255, 255, 255, 255, 255, 255 },
            { 0, 0, 0, 0, 0, 0, 0, 255 },
            { 0, 255, 255, 255, 255, 255, 0, 255 },
            { 0, 255, 0, 0, 0, 255, 0, 255 },
            { 0, 255, 0, 0, 0, 255, 0, 255 },
            { 0, 255, 0, 0, 0, 255, 0, 255 },
            { 0, 255, 255, 255, 255, 255, 0, 255 },
            { 0, 0, 0, 0, 0, 0, 0, 255 },
        } } }
    };

    // Reserving (9x9 for each), (which then also includes reserving the format information) and
    // placing finder patterns: 9x9 for top left, 9x8 for top right and 8x9 for the bottom left one

    size_t module_idx = 0;
    for (size_t i = 0; i < 3; ++i) {
      size_t                                rows{ static_cast<size_t>(i != 2 ? 9 : 8) };
      std::array<std::array<uint8_t, 8>, 8> fp_modules{ fp_modules_to_fill[i] };

      for (size_t r = 0; r < rows; ++r) {
        m_reservedModules_[module_idx] = 1;
        m_reservedModules_[module_idx + 1] = 1;
        m_reservedModules_[module_idx + 2] = 1;
        m_reservedModules_[module_idx + 3] = 1;
        m_reservedModules_[module_idx + 4] = 1;
        m_reservedModules_[module_idx + 5] = 1;
        m_reservedModules_[module_idx + 6] = 1;
        m_reservedModules_[module_idx + 7] = 1;
        if (i != 1)
          m_reservedModules_[module_idx + 8] = 1;

        if (r < 8) {
          std::array<uint8_t, 8> row_modules{ fp_modules[r] };
          for (size_t m = 0; m < 8; ++m) m_rawModules_[module_idx + m] = row_modules[m];
        }

        module_idx += modules_width;
      }

      if (i == 0)
        module_idx = modules_width - 8;
      else if (i == 1)
        module_idx = (modules_width - 8) * modules_width;
    }

    static constexpr std::array<std::array<uint8_t, 5>, 5> ap_modules_to_fill{ {
        { 0, 0, 0, 0, 0 },
        { 0, 255, 255, 255, 0 },
        { 0, 255, 0, 255, 0 },
        { 0, 255, 255, 255, 0 },
        { 0, 0, 0, 0, 0 },
    } };

    // Version 1 doesn't have any alignment patterns
    if (version > 1) {
      // Reserving and placing every alignment patterns: 5x5 each
      size_t patterns_per_row{ static_cast<size_t>(std::floor(static_cast<double>(version) / 7) +
                                                   2) };

      /*
        Well, uhm, some math... I'll try to explain it:

        The first and last pattern column is the center column of the first and last row of
        alignment patterns (horizontally) are located at, suppose:

        modules_width = 45, so the modules go through 0-44

        - - - - - - - - - - .. (alignment patterns) .. - - -  -  -  -  -  -  -
        - - - - D D D D D - .......................... D D D  D  D  -  -  -  -
        - - - - D W W W D - .......................... D W W  W  D  -  -  -  -
        - - - - D W D W D - ........ Distance ........ D W D  W  D  -  -  -  -
        0 1 2 3 4 5 6     - ..........................     38 39 40 41 42 43 44
        - - - - D W W W D - .......................... D W W  W  D  -  -  -  -
        - - - - D D D D D - .......................... D D D  D  D  -  -  -  -
        - - - - - - - - - - .. (alignment patterns) .. - - -  -  -  -  -  -  -

        Get the distance (38 - 6) and divice that by the total patterns - 1, I don't know why to be
        honest. Lastly round it up to the next multiply of 2. And that's the column distance between
        the last alignment patterns and the ones before. Remember that the center of the first and
        last row of alignment pattern must always be 'first_center_column' and 'last_center_column'
        - and don't overwrite finder patterns!
      */

      size_t first_center_column{ 6 };
      size_t last_center_column{ modules_width - 7 };

      size_t bottom_row{ modules_width - 9 };

      size_t odd_dist_between_patterns{ (last_center_column - first_center_column) /
                                        (patterns_per_row - 1) };
      size_t dist_between_patterns{ (odd_dist_between_patterns + 1) -
                                    ((odd_dist_between_patterns + 1) % 2) };

      // I want to iterate from the start and not the center
      last_center_column -= 2;

      for (size_t r = 0; r < patterns_per_row; ++r) {
        size_t last_pattern{ patterns_per_row - 1 };
        size_t row{ r == last_pattern ? 4 : bottom_row - (dist_between_patterns * r) };

        bool is_first_row{ r == 0 };
        bool is_last_row{ r == last_pattern };

        size_t row_module_idx{ row * modules_width };

        for (size_t c = 0; c < patterns_per_row; ++c) {
          // Skip finder patterns
          if ((c == last_pattern && (is_last_row || is_first_row)) || c == 0 && is_last_row)
            continue;

          size_t column{ c == 0              ? last_center_column
                         : c == last_pattern ? 4
                                             : last_center_column - (dist_between_patterns * c) };

          size_t module_idx{ row_module_idx + column };
          for (size_t pr = 0; pr < 5; ++pr) {
            m_reservedModules_[module_idx] = 1;
            m_reservedModules_[module_idx + 1] = 1;
            m_reservedModules_[module_idx + 2] = 1;
            m_reservedModules_[module_idx + 3] = 1;
            m_reservedModules_[module_idx + 4] = 1;

            std::array<uint8_t, 5> to_fill{ ap_modules_to_fill[pr] };
            for (size_t m = 0; m < 5; ++m) m_rawModules_[module_idx + m] = to_fill[m];
            module_idx += modules_width;
          }
        }
      }
    }

    size_t version_mul4{ version * 4 };

    // Reserving timing patterns
    reserve_area(6, 9, version_mul4, 1, modules_width);
    reserve_area(9, 6, 1, version_mul4, modules_width);

    // Placing the timing patterns (horizontally and vertically). They alternate between dark and
    // light modules
    size_t tp_end{ 2 + version_mul4 };

    size_t h_idx{ (6 * modules_width) + 6 };
    size_t v_idx{ (6 * modules_width) + 6 };
    size_t h_stride{ modules_width * 2 };
    size_t v_stride{ 2 };

    for (size_t i = 0; i < tp_end; i += 2) {
      m_rawModules_[v_idx += v_stride] = 0;
      m_rawModules_[h_idx += h_stride] = 0;
    }

    // Version 7 and below don't have version information
    if (version >= 7) {
      // Reserving version information modules
      reserve_area(modules_width - 11, 0, 6, 3, modules_width);
      reserve_area(0, modules_width - 11, 3, 6, modules_width);
    }

    // Reserving dark module
    module_idx = ((modules_width - 8) * modules_width) + 8;
    m_rawModules_[module_idx] = 0;
    m_reservedModules_[module_idx] = 1;
  }

  void reserve_area(const size_t row, const size_t column, const size_t width, const size_t height,
                    const size_t modules_width) {
    for (size_t r = row; r < row + height; ++r) {
      size_t row_module_idx = r * modules_width;
      for (size_t c = column; c < column + width; ++c) m_reservedModules_[row_module_idx + c] = 1;
    }
  }

  void place_interleaved_codewords(const size_t modules_width, const size_t total_modules,
                                   bool debug) {
    // TODO: Should replace the asserts with a proper function (probably just taking a bool and a
    // literal char*) and add a nice description of the algorithm.

    size_t module_idx{ total_modules };

    CodewordBitReader bits{ m_interleavedCodewords_, debug };

    m_rawModules_[--module_idx] = bits.next();
    m_rawModules_[--module_idx] = bits.next();

    bool going_up = true;
    bool may_move_left = false;

    size_t last_row_module_idx = total_modules - modules_width;

    auto check_row_availability = [&](bool assert_if_not_found = false) {
      if (going_up) {
        for (size_t i = module_idx; i >= modules_width;) {
          i -= modules_width;

          if (!m_reservedModules_[i]) {
            module_idx = i + modules_width;
            return true;
          }

          if (i < modules_width) {
            assert(!assert_if_not_found);

            if (!m_reservedModules_[i - 2]) {
              module_idx = i - 2;
              m_rawModules_[module_idx] = bits.next();

              going_up = false;
              return true;
            }
          }
        }
      } else {
        for (size_t i = module_idx + modules_width; i < total_modules; i += modules_width) {
          if (!m_reservedModules_[i]) {
            module_idx = i - modules_width;
            return true;
          }

          if (i >= last_row_module_idx) {
            assert(!assert_if_not_found);

            if (!m_reservedModules_[i - 2]) {
              module_idx = i - 2;
              m_rawModules_[module_idx] = bits.next();

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
        m_rawModules_[--module_idx] = bits.next();
      may_move_left = true;

      if (!bits.in_bounds())
        break;

      size_t next_module_idx =
          going_up ? (module_idx - modules_width) : (module_idx + modules_width);

      bool next_row_in_bounds = (going_up && (module_idx > modules_width)) ||
                                (!going_up && (module_idx <= last_row_module_idx));

      bool can_move_to_next_row = next_row_in_bounds && !m_reservedModules_[next_module_idx];
      bool can_move_diagonal = can_move_to_next_row && !m_reservedModules_[next_module_idx + 1];

      if (can_move_diagonal) {
        module_idx = next_module_idx;
        m_rawModules_[++module_idx] = bits.next();
        continue;
      }

      if (can_move_to_next_row) {
        module_idx = next_module_idx;
        m_rawModules_[module_idx] = bits.next();
        may_move_left = false;
        continue;
      }

      if (check_row_availability()) {
        may_move_left = false;
        continue;
      }

      --module_idx;
      bool can_move_left = !m_reservedModules_[module_idx];

      if (can_move_left)
        m_rawModules_[module_idx] = bits.next();
      else {
        if (!m_reservedModules_[module_idx - 1]) {
          --module_idx;
          m_rawModules_[module_idx] = bits.next();
        } else {
          going_up = !going_up;
          check_row_availability(true);
          m_rawModules_[module_idx -= modules_width] = bits.next();
          continue;
        }
      }

      going_up = !going_up;
    }
  }

  template <bool WriteRaw>
  void place_format_info_modules(const uint8_t mask_idx, const Ecc ecc, const size_t modules_width,
                                 const bool debug) {
    /*

        Format information modules placement:

        X = finder pattern module
        T = timing pattern module
        D = the one reserved always dark module

        X X X X X X X X 14 - - - - - - - - - - X X X  X  X  X  X  X
        X X X X X X X X 13 - - - - - - - - - - X X X  X  X  X  X  X
        X X X X X X X X 12 - - - - - - - - - - X X X  X  X  X  X  X
        X X X X X X X X 11 - - - - - - - - - - X X X  X  X  X  X  X
        X X X X X X X X 10 - - - - - - - - - - X X X  X  X  X  X  X
        X X X X X X X X 9  - - - - - - - - - - X X X  X  X  X  X  X
        X X X X X X X X T  T T T T T T T T T T X X X  X  X  X  X  X
        X X X X X X X X 8  - - - - - - - - - - X X X  X  X  X  X  X
        0 1 2 3 4 5 T 6 7  - - - - - - - - - - 7 8 9 10 11 12 13 14
        - - - - - - T - -  - - - - - - - - - - - - -  -  -  -  -  -
        - - - - - - T - -  - - - - - - - - - - - - -  -  -  -  -  -
        - - - - - - T - -  - - - - - - - - - - - - -  -  -  -  -  -
        - - - - - - T - -  - - - - - - - - - - - - -  -  -  -  -  -
        - - - - - - T - -  - - - - - - - - - - - - -  -  -  -  -  -
        - - - - - - T - -  - - - - - - - - - - - - -  -  -  -  -  -
        - - - - - - T - -  - - - - - - - - - - - - -  -  -  -  -  -
        - - - - - - T - -  - - - - - - - - - - - - -  -  -  -  -  -
        - - - - - - T - -  - - - - - - - - - - - - -  -  -  -  -  -
        - - - - - - T - -  - - - - - - - - - - - - -  -  -  -  -  -
        - - - - - - T - -  - - - - - - - - - - - - -  -  -  -  -  -
        X X X X X X X X D  - - - - - - - - - - - - -  -  -  -  -  -
        X X X X X X X X 6  - - - - - - - - - - - - -  -  -  -  -  -
        X X X X X X X X 5  - - - - - - - - - - - - -  -  -  -  -  -
        X X X X X X X X 4  - - - - - - - - - - - - -  -  -  -  -  -
        X X X X X X X X 3  - - - - - - - - - - - - -  -  -  -  -  -
        X X X X X X X X 2  - - - - - - - - - - - - -  -  -  -  -  -
        X X X X X X X X 1  - - - - - - - - - - - - -  -  -  -  -  -
        X X X X X X X X 0  - - - - - - - - - - - - -  -  -  -  -  -

    */

    std::array<uint8_t, 15> format_info =
        c_formatInformationTable[debug ? 4 : static_cast<uint8_t>(ecc)][debug ? 0 : mask_idx];

    size_t module_idx = (8 * modules_width);

    // TODO: I can do better.
    if (WriteRaw) {
      m_rawModules_[module_idx] = format_info[0];
      m_rawModules_[module_idx += 1] = format_info[1];
      m_rawModules_[module_idx += 1] = format_info[2];
      m_rawModules_[module_idx += 1] = format_info[3];
      m_rawModules_[module_idx += 1] = format_info[4];
      m_rawModules_[module_idx += 1] = format_info[5];
      m_rawModules_[module_idx += 2] = format_info[6];
      m_rawModules_[module_idx += 1] = format_info[7];

      m_rawModules_[module_idx -= modules_width] = format_info[8];
      m_rawModules_[module_idx -= (modules_width * 2)] = format_info[9];
      m_rawModules_[module_idx -= modules_width] = format_info[10];
      m_rawModules_[module_idx -= modules_width] = format_info[11];
      m_rawModules_[module_idx -= modules_width] = format_info[12];
      m_rawModules_[module_idx -= modules_width] = format_info[13];
      m_rawModules_[module_idx -= modules_width] = format_info[14];

      module_idx = ((modules_width - 1) * modules_width) + 8;

      m_rawModules_[module_idx] = format_info[0];
      m_rawModules_[module_idx -= modules_width] = format_info[1];
      m_rawModules_[module_idx -= modules_width] = format_info[2];
      m_rawModules_[module_idx -= modules_width] = format_info[3];
      m_rawModules_[module_idx -= modules_width] = format_info[4];
      m_rawModules_[module_idx -= modules_width] = format_info[5];
      m_rawModules_[module_idx -= modules_width] = format_info[6];

      module_idx = (8 * modules_width) + (modules_width - 8);

      m_rawModules_[module_idx] = format_info[7];
      m_rawModules_[module_idx += 1] = format_info[8];
      m_rawModules_[module_idx += 1] = format_info[9];
      m_rawModules_[module_idx += 1] = format_info[10];
      m_rawModules_[module_idx += 1] = format_info[11];
      m_rawModules_[module_idx += 1] = format_info[12];
      m_rawModules_[module_idx += 1] = format_info[13];
      m_rawModules_[module_idx += 1] = format_info[14];
    } else {
      m_maskedModules_[module_idx] = format_info[0] == 0 ? 1 : 0;
      m_maskedModules_[module_idx += 1] = format_info[1] == 0 ? 1 : 0;
      m_maskedModules_[module_idx += 1] = format_info[2] == 0 ? 1 : 0;
      m_maskedModules_[module_idx += 1] = format_info[3] == 0 ? 1 : 0;
      m_maskedModules_[module_idx += 1] = format_info[4] == 0 ? 1 : 0;
      m_maskedModules_[module_idx += 1] = format_info[5] == 0 ? 1 : 0;
      m_maskedModules_[module_idx += 2] = format_info[6] == 0 ? 1 : 0;
      m_maskedModules_[module_idx += 1] = format_info[7] == 0 ? 1 : 0;

      m_maskedModules_[module_idx -= modules_width] = format_info[8] == 0 ? 1 : 0;
      m_maskedModules_[module_idx -= (modules_width * 2)] = format_info[9] == 0 ? 1 : 0;
      m_maskedModules_[module_idx -= modules_width] = format_info[10] == 0 ? 1 : 0;
      m_maskedModules_[module_idx -= modules_width] = format_info[11] == 0 ? 1 : 0;
      m_maskedModules_[module_idx -= modules_width] = format_info[12] == 0 ? 1 : 0;
      m_maskedModules_[module_idx -= modules_width] = format_info[13] == 0 ? 1 : 0;
      m_maskedModules_[module_idx -= modules_width] = format_info[14] == 0 ? 1 : 0;

      module_idx = ((modules_width - 1) * modules_width) + 8;

      m_maskedModules_[module_idx] = format_info[0];
      m_maskedModules_[module_idx -= modules_width] = format_info[1] == 0 ? 1 : 0;
      m_maskedModules_[module_idx -= modules_width] = format_info[2] == 0 ? 1 : 0;
      m_maskedModules_[module_idx -= modules_width] = format_info[3] == 0 ? 1 : 0;
      m_maskedModules_[module_idx -= modules_width] = format_info[4] == 0 ? 1 : 0;
      m_maskedModules_[module_idx -= modules_width] = format_info[5] == 0 ? 1 : 0;
      m_maskedModules_[module_idx -= modules_width] = format_info[6] == 0 ? 1 : 0;

      module_idx = (8 * modules_width) + (modules_width - 8);

      m_maskedModules_[module_idx] = format_info[7] == 0 ? 1 : 0;
      m_maskedModules_[module_idx += 1] = format_info[8] == 0 ? 1 : 0;
      m_maskedModules_[module_idx += 1] = format_info[9] == 0 ? 1 : 0;
      m_maskedModules_[module_idx += 1] = format_info[10] == 0 ? 1 : 0;
      m_maskedModules_[module_idx += 1] = format_info[11] == 0 ? 1 : 0;
      m_maskedModules_[module_idx += 1] = format_info[12] == 0 ? 1 : 0;
      m_maskedModules_[module_idx += 1] = format_info[13] == 0 ? 1 : 0;
      m_maskedModules_[module_idx += 1] = format_info[14] == 0 ? 1 : 0;
    }
  }

  void place_version_info_modules(const size_t version_idx, const size_t modules_width,
                                  const bool debug) {
    std::array<uint8_t, 18> version_info{ c_versionInformationTable[debug ? 0 : version_idx] };

    size_t module_idx{ (modules_width - 11) * modules_width };

    // TODO: Add a nice graphic like in info module, I should become a graphics designer.

    m_rawModules_[module_idx] = version_info[0];
    m_rawModules_[module_idx + 1] = version_info[3];
    m_rawModules_[module_idx + 2] = version_info[6];
    m_rawModules_[module_idx + 3] = version_info[9];
    m_rawModules_[module_idx + 4] = version_info[12];
    m_rawModules_[module_idx + 5] = version_info[15];

    module_idx += modules_width;

    m_rawModules_[module_idx] = version_info[1];
    m_rawModules_[module_idx + 1] = version_info[4];
    m_rawModules_[module_idx + 2] = version_info[7];
    m_rawModules_[module_idx + 3] = version_info[10];
    m_rawModules_[module_idx + 4] = version_info[13];
    m_rawModules_[module_idx + 5] = version_info[16];

    module_idx += modules_width;

    m_rawModules_[module_idx] = version_info[2];
    m_rawModules_[module_idx + 1] = version_info[5];
    m_rawModules_[module_idx + 2] = version_info[8];
    m_rawModules_[module_idx + 3] = version_info[11];
    m_rawModules_[module_idx + 4] = version_info[14];
    m_rawModules_[module_idx + 5] = version_info[17];

    module_idx = modules_width - 11;
    m_rawModules_[module_idx] = version_info[0];
    m_rawModules_[module_idx + 1] = version_info[1];
    m_rawModules_[module_idx + 2] = version_info[2];

    module_idx += modules_width;
    m_rawModules_[module_idx] = version_info[3];
    m_rawModules_[module_idx + 1] = version_info[4];
    m_rawModules_[module_idx + 2] = version_info[5];

    module_idx += modules_width;
    m_rawModules_[module_idx] = version_info[6];
    m_rawModules_[module_idx + 1] = version_info[7];
    m_rawModules_[module_idx + 2] = version_info[8];

    module_idx += modules_width;
    m_rawModules_[module_idx] = version_info[9];
    m_rawModules_[module_idx + 1] = version_info[10];
    m_rawModules_[module_idx + 2] = version_info[11];

    module_idx += modules_width;
    m_rawModules_[module_idx] = version_info[12];
    m_rawModules_[module_idx + 1] = version_info[13];
    m_rawModules_[module_idx + 2] = version_info[14];

    module_idx += modules_width;
    m_rawModules_[module_idx] = version_info[15];
    m_rawModules_[module_idx + 1] = version_info[16];
    m_rawModules_[module_idx + 2] = version_info[17];
  }

  void apply_best_mask(const size_t modules_width, const size_t total_modules, const Ecc ecc,
                       bool debug) {
    uint8_t best_mask = 0;
    size_t  lowest_penalty = SIZE_MAX;

    for (uint8_t i = 0; i < 8; ++i) {
      mask_data_pixels<false>(i, modules_width, total_modules);
      place_format_info_modules<false>(i, ecc, modules_width, debug);

      size_t penalty = compute_mask_penalty(modules_width, total_modules);
      if (penalty < lowest_penalty) {
        best_mask = i;
        lowest_penalty = penalty;
      }
    }

    std::cout << "[Debug] QrCodeGenerator -> Applied best mask index "
              << static_cast<size_t>(best_mask) << ".\n";

    mask_data_pixels<true>(best_mask, modules_width, total_modules);
    place_format_info_modules<true>(best_mask, ecc, modules_width, debug);
  }

  template <bool WriteRaw>
  void mask_data_pixels(const uint8_t mask_idx, const size_t modules_width,
                        const size_t total_modules) {
    /*
      Simply performs a mask function on every module which is not reserved (data and error
      correction data). WriteRaw is used to avoid copying from 'm_maskedModules_' to
      'm_rawModules' once the best mask index is choosen.
    */

    auto mask_fn = c_maskFns_[mask_idx];

    size_t row = 0;
    size_t column = 0;

    size_t row_module_idx{ modules_width };

    for (size_t i = 0; i < total_modules; ++i, ++column) {
      uint8_t value = m_maskedModules_[i];
      if (i >= row_module_idx) {
        ++row;
        column = 0;
        row_module_idx += modules_width;
      }

      uint8_t module{ static_cast<uint8_t>(m_rawModules_[i] == 0 ? 1 : 0) };
      if (m_reservedModules_[i]) {
        m_maskedModules_[i] = module;
        continue;
      }

      if (WriteRaw)
        m_rawModules_[i] = ((module ^ mask_fn(row, column)) == 1) ? 0 : 255;
      else
        m_maskedModules_[i] = module ^ mask_fn(row, column);
    }
  }

  // TODO: It's not correct, its off by a bit, but it's so tedious to test, I don't really care. But
  // sometime?
  size_t compute_mask_penalty(const size_t modules_width, const size_t total_modules) {
    /*
      The QR Specification requires to pick the best mask index 0-7 with the lowest penalty based on
      some rules. So there are 4 rules specified to calculate the penalty of each mask index.

      Rule 1:
        Each horizontal or vertical sequence of the same value, whichs length is greater or equal to
      5 (D D D D D or L L L L L L) adds the sequence length - 2 to the penalty score.

      Rule 2:
        Each 2x2 square of the same value, including overlapping squares, adds +3 to the penalty
      score.

      Rule 3:
        Each sequence that matches this pattern (L L L L D L D D D L D) or its reverse (D L D D D L
      D L L L L), adds +40 to the penalty score.

      Rule 4:
        From the total modules, get the percentage of dark modules. If it's greater than 50, round
      it down to the next multiply of 5, else round it up to the next multiply of 5. Now from 50
      subtract the value and multiply the absolute (-X->+X,+X->+X) result by 2. Add that to the
      penalty score.
    */

    size_t penalty{ 0 };

    size_t  sequence_length{ 0 };
    uint8_t cur_counting_value{ 0 };
#
    auto apply_rule1 = [&](const uint8_t value, const bool new_rule) {
      if (value != cur_counting_value || new_rule) {
        cur_counting_value = value;
        sequence_length = 1;
        return;
      }

      ++sequence_length;
      if (sequence_length == 5)
        penalty += 3;
      else if (sequence_length > 5)
        ++penalty;
    };

    auto apply_rule2 = [&](const size_t module_idx, const uint8_t value,
                           const size_t row_module_idx) {
      size_t bottom_right_idx{ module_idx + modules_width + 1 };
      if ((bottom_right_idx > total_modules) || (module_idx >= row_module_idx - 1))
        return;

      if (m_maskedModules_[module_idx + 1] == value &&
          m_maskedModules_[module_idx + modules_width] == value &&
          m_maskedModules_[bottom_right_idx] == value)
        penalty += 3;
    };

    size_t total_dark_modules{ 0 };

    size_t row_module_idx{ modules_width };
    for (size_t i = 0; i < total_modules; ++i) {
      uint8_t value = m_maskedModules_[i];
      if (value == 1)
        ++total_dark_modules;

      bool new_row = false;
      if (i >= row_module_idx) {
        new_row = true;
        row_module_idx += modules_width;
      }

      apply_rule1(value, new_row);
      apply_rule2(i, value, row_module_idx);
      apply_rule3<true>(i, row_module_idx, value, modules_width, total_modules, &penalty);
    }

    sequence_length = 0;
    cur_counting_value = 0;

    for (size_t c = 0; c < modules_width; ++c) {
      size_t module_idx{ c };
      for (size_t r = 0; r < modules_width; ++r) {
        uint8_t value = m_maskedModules_[module_idx];
        apply_rule1(value, false);
        apply_rule3<false>(module_idx, 0, value, modules_width, total_modules, &penalty);
        module_idx += modules_width;
      }
    }

    size_t dark_modules_percentage{ static_cast<size_t>(
        std::floor((total_dark_modules / static_cast<double>(total_modules)) * 100)) };

    if (dark_modules_percentage > 50)
      dark_modules_percentage -= (dark_modules_percentage % 5);
    else
      dark_modules_percentage = (dark_modules_percentage + 4) - ((dark_modules_percentage + 4) % 5);
    return penalty + (std::abs(static_cast<int32_t>(50 - dark_modules_percentage)) * 2);
  }

  template <bool ApplyForColumn>
  void apply_rule3(const size_t c_module_idx, const size_t row_module_idx, const uint8_t value,
                   const size_t modules_width, const size_t total_modules, size_t* penalty) {
    if (ApplyForColumn ? ((c_module_idx + 11) > row_module_idx)
                       : ((c_module_idx * 11) >= total_modules))
      return;

    const size_t stride = ApplyForColumn ? 1 : modules_width;
    size_t       module_idx = c_module_idx;

    uint8_t module_0 = value;
    uint8_t module_1 = m_maskedModules_[module_idx += stride];
    uint8_t module_2 = m_maskedModules_[module_idx += stride];
    uint8_t module_3 = m_maskedModules_[module_idx += stride];

    if (module_0 == 0 && module_1 == 0 && module_2 == 0 && module_3 == 0) {
      uint8_t module_4 = m_maskedModules_[module_idx += stride];
      uint8_t module_5 = m_maskedModules_[module_idx += stride];
      uint8_t module_6 = m_maskedModules_[module_idx += stride];
      uint8_t module_7 = m_maskedModules_[module_idx += stride];

      if (module_4 != 1 || module_5 != 0 || module_6 != 1 || module_7 != 1)
        return;

      uint8_t module_8 = m_maskedModules_[module_idx += stride];
      uint8_t module_9 = m_maskedModules_[module_idx += stride];
      uint8_t module_10 = m_maskedModules_[module_idx += stride];

      if (module_8 == 1 && module_9 == 0 && module_10 == 1)
        *penalty += 40;
      return;
    }

    if (module_0 == 1 && module_1 == 0 && module_2 == 1 && module_3 == 1) {
      uint8_t module_4 = m_maskedModules_[module_idx += stride];
      uint8_t module_5 = m_maskedModules_[module_idx += stride];
      uint8_t module_6 = m_maskedModules_[module_idx += stride];
      uint8_t module_7 = m_maskedModules_[module_idx += stride];

      if (module_4 != 1 || module_5 != 0 || module_6 != 1 || module_7 != 0)
        return;

      uint8_t module_8 = m_maskedModules_[module_idx += stride];
      uint8_t module_9 = m_maskedModules_[module_idx += stride];
      uint8_t module_10 = m_maskedModules_[module_idx += stride];

      if (module_8 == 0 && module_9 == 0 && module_10 == 0)
        *penalty += 40;
    }
  }

  uint8_t gf256_mul(uint8_t a, uint8_t b) const {
    // A special way of multiplication which basically maps the result of A * B to something
    // between 0 and 255, which is required for further operations on the product of large values
    // as a byte
    return c_expTable[(c_logTable[a] + c_logTable[b]) % 255];
  }

  void poly_div(size_t data_codewords_per_block, size_t total_codewords_per_block) {
    assert(m_polyDivDataSize_ == total_codewords_per_block);
    assert(total_codewords_per_block >= data_codewords_per_block);

    /*
      Polynomial division using GF(256) multiplication.

      Iterates through 'm_polyDivData_' which must be set before using
      'set_polynomial_division_data' and check if it's non-zero, otherwise it just continues,
      let's say that's X and then performs for the next N (m_polyCoeffsSize_) bytes (including X)
      a XOR with the multiplication of X and the current coefficient (m_polyCoeffs_, 0-N) in
      GF(256)
    */
    for (size_t i = 0; i < data_codewords_per_block; ++i) {
      uint8_t start{ m_polyDivData_[i] };
      if (start != 0) {
        for (size_t g = 0; g < m_polyCoeffsSize_; ++g)
          m_polyDivData_[i + g] ^= gf256_mul(start, m_polyCoeffs_[g]);
      }
    }
  }

  void set_poly_div_data(size_t data_codewords_per_block, size_t total_codewords_per_block,
                         size_t offset) {
    assert(m_rawCodewords_.size >= offset);
    assert(total_codewords_per_block >= data_codewords_per_block);

    for (size_t i = 0; i < total_codewords_per_block; ++i)
      m_polyDivData_[i] = (i >= data_codewords_per_block ? 0 : m_rawCodewords_[i + offset]);
    m_polyDivDataSize_ = total_codewords_per_block;
  }

 private:
  static constexpr std::array<uint8_t (*)(size_t, size_t), 8> c_maskFns_ = {
    [](size_t row, size_t column) -> uint8_t { return ((row + column) % 2) == 0; },
    [](size_t row, size_t column) -> uint8_t { return (row % 2) == 0; },
    [](size_t row, size_t column) -> uint8_t { return (column % 3) == 0; },
    [](size_t row, size_t column) -> uint8_t { return ((row + column) % 3) == 0; },
    [](size_t row, size_t column) -> uint8_t {
      return ((static_cast<size_t>(std::floor(row / 2)) +
               static_cast<size_t>(std::floor(column / 3))) %
              2) == 0;
    },
    [](size_t row, size_t column) -> uint8_t { return (row * column % 2 + row * column % 3) == 0; },
    [](size_t row, size_t column) -> uint8_t {
      return (((row * column) % 2 + row * column % 3) % 2) == 0;
    },
    [](size_t row, size_t column) -> uint8_t {
      return (((row + column) % 2 + row * column % 3) % 2) == 0;
    }
  };

  // TODO: get max total_modules
  std::array<uint8_t, 50000> m_rawModules_;
  std::array<uint8_t, 50000> m_maskedModules_;
  std::bitset<50000>         m_reservedModules_;

  // TODO: whats the max?
  CodewordArray m_rawCodewords_{};
  CodewordArray m_interleavedCodewords_{};

  std::array<uint8_t, c_maxPolyCoeffs> m_polyCoeffs_{};
  size_t                               m_polyCoeffsSize_ = 0;

  std::array<uint8_t, c_maxPolyDivData> m_polyDivData_{};
  size_t                                m_polyDivDataSize_ = 0;
};

#endif