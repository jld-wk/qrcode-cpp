// SPDX-FileCopyrightText: 2026 Julian Duwe
// SPDX-License-Identifier: Apache-2.0

#ifndef JLD_QRCODE_HPP
#define JLD_QRCODE_HPP

#include <algorithm>
// NOLINTNEXTLINE
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#include <array>
#include <bitset>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <vector>

#include "common.hpp"
#include "memoized.hpp"

namespace jld {

using CodewordArray = ByteArray<c_maxCodewordsCount>;
using ModulesArray = std::array<uint8_t, c_maxModulesCount>;

class QrCodeDebugFlag {
 public:
  static constexpr uint32_t c_none_ = 0;
  static constexpr uint32_t c_debugFormatInfo_ = 1;
  static constexpr uint32_t c_debugVersionInfo_ = 1 << 1;
  static constexpr uint32_t c_debugTimingPatterns_ = 1 << 2;
  static constexpr uint32_t c_printInBinary_ = 1 << 3;
  static constexpr uint32_t c_printRawCodewords_ = 1 << 4;
  static constexpr uint32_t c_printInterleavedCodewords_ = 1 << 5;
  static constexpr uint32_t c_printErrorCorrectionData_ = 1 << 6;
  static constexpr uint32_t c_disableMasking_ = 1 << 7;
  static constexpr uint32_t c_debugBitPlacing_ = 1 << 9;
  static constexpr uint32_t c_disableBitPlacing_ = 1 << 10;
  static constexpr uint32_t c_debugReservedModules_ = 1 << 11;
  static constexpr uint32_t c_printBestMaskIdx_ = 1 << 12;

  const uint32_t mask = c_none_;

  // NOLINTNEXTLINE
  constexpr QrCodeDebugFlag(uint32_t mask)
      : mask{ mask } {}

  // NOLINTNEXTLINE
  [[nodiscard]] constexpr operator uint32_t() {
    return mask;
  }

  [[nodiscard]] constexpr auto operator|(uint32_t b) const -> QrCodeDebugFlag {
    return QrCodeDebugFlag{ mask | b };
  }

  [[nodiscard]] constexpr auto operator&(uint32_t b) const -> QrCodeDebugFlag {
    return QrCodeDebugFlag{ mask & b };
  }
};

class QrCodeGenInfoFlag {
 public:
  static constexpr uint32_t c_none_ = 0;
  static constexpr uint32_t c_useCustomMaskIdx_ = 1 << 8;

  const uint32_t mask = c_none_;

  // NOLINTNEXTLINE
  constexpr QrCodeGenInfoFlag(uint32_t mask)
      : mask{ mask } {}

  // NOLINTNEXTLINE
  [[nodiscard]] constexpr operator uint32_t() const {
    return mask;
  }

  [[nodiscard]] constexpr auto operator|(uint32_t b) const -> QrCodeGenInfoFlag {
    return QrCodeGenInfoFlag{ mask | b };
  }

  [[nodiscard]] constexpr auto operator&(uint32_t b) const -> QrCodeGenInfoFlag {
    return QrCodeGenInfoFlag{ mask & b };
  }
};

enum class QrCodeEcc : uint8_t { M, L, H, Q };
enum class QrCodeBitLen : uint8_t { Bits8, Bits16 };

struct QrCodeGenInfo {
  QrCodeEcc         ecc = QrCodeEcc::M;
  uint8_t           version = 1;
  const char*       outPath = nullptr;
  uint8_t           customMaskIdx = 0;
  QrCodeGenInfoFlag flags = QrCodeGenInfoFlag::c_none_;
};

enum class QrCodeGenResult : uint8_t {
  Success,

  /*
    These results represent invalid input from the user
  */
  DataTooLarge,
  InvalidVersion,
  InvalidCustomMaskIdx,

  /*
    stbi_write_jpg failed
  */
  StbFailure,

  /*
    These results represent that the memoized data (memoized.hpp) is invalid or the constants inside
    common.hpp have been modified without caution
  */
  AssertFailure0,
  AssertFailure1,
  AssertFailure2,
  AssertFailure3,
  AssertFailure4,
  AssertFailure5,
  AssertFailure6,
  AssertFailure7,
  AssertFailure8,
  AssertFailure9,
  AssertFailure10,
  AssertFailure11
};

#define JLD_QRCODE_ASSERT(condition, result, message)               \
  if (!(condition)) {                                               \
    std::cerr << "[ASSERT] jldQrCodeGen -> " << (message) << ".\n"; \
    assert(false);                                                  \
    return result;                                                  \
  }

class QrCodeGenerator {
 public:
  QrCodeGenerator() {
    for (uint8_t& e : m_rawModules_) e = 255;
    for (uint8_t& e : m_maskedModules_) e = 255;
  }

  template <QrCodeDebugFlag DebugFlags = QrCodeDebugFlag::c_none_>
  [[nodiscard]] auto generate(const std::vector<uint8_t>& data, const QrCodeGenInfo& gen_info)
      -> QrCodeGenResult {
    if (gen_info.version == 0 || gen_info.version > 40)
      return QrCodeGenResult::InvalidVersion;

    const uint8_t version_idx = gen_info.version - 1;
    JLD_QRCODE_ASSERT(version_idx < c_qrEncodingInfoTable.size(), QrCodeGenResult::AssertFailure0,
                      "[generate] version_idx exceeds c_qrEncodingInfoTable");

    const QrCodeEncodingInfo enc_info =
        c_qrEncodingInfoTable[version_idx][static_cast<size_t>(gen_info.ecc)];

    const uint32_t blocks_count = enc_info.g1BlocksCount + enc_info.g2BlocksCount;
    const uint32_t codewords_count = enc_info.ecCodewordsCount + enc_info.dataCodewordsCount;

    const uint8_t g2_data_codewords_count = enc_info.g1DataCodewordsCount + 1;

    static_assert(c_maxModulesCount == sizeof(ModulesArray),
                  "c_maxCodewordsCount must equal ModulesArray size");
    static_assert(c_maxCodewordsCount == CodewordArray::c_size_,
                  "c_maxCodewordsCount must equal CodewordArray size");

    JLD_QRCODE_ASSERT(enc_info.modulesCount <= c_maxModulesCount, QrCodeGenResult::AssertFailure1,
                      "[generate] modulesCount exceeds c_maxModulesCount");
    JLD_QRCODE_ASSERT((enc_info.modulesWidth * enc_info.modulesWidth) == enc_info.modulesCount,
                      QrCodeGenResult::AssertFailure2,
                      "[generate] (modulesWidth * modulesWidth) must equal modulesCount");
    JLD_QRCODE_ASSERT(codewords_count <= c_maxCodewordsCount, QrCodeGenResult::AssertFailure3,
                      "[generate] codewords_count exceeds c_maxCodewordsCount");
    JLD_QRCODE_ASSERT(
        (enc_info.g1BlocksCount * static_cast<uint32_t>(enc_info.g1DataCodewordsCount) +
         enc_info.g2BlocksCount * static_cast<uint32_t>(g2_data_codewords_count)) ==
            enc_info.dataCodewordsCount,
        QrCodeGenResult::AssertFailure4,
        "[generate] (g1BlocksCount * g1DataCodewordsCount + g2BlocksCount * "
        "g2_data_codewords_count) must equal "
        "dataCodewordsCount");
    // NOLINTNEXTLINE
    JLD_QRCODE_ASSERT(
        (enc_info.dataCodewordsCount == enc_info.writableDataCodewordsCount + 2) ||
            (enc_info.dataCodewordsCount == enc_info.writableDataCodewordsCount + 3),
        QrCodeGenResult::AssertFailure5,
        "[generate] dataCodewordsCount must equal (writableDataCodewordsCount +2 or +3)");
    JLD_QRCODE_ASSERT(
        (enc_info.ecCodewordsCount / blocks_count) == enc_info.g1g2EcCodewordsCount,
        QrCodeGenResult::AssertFailure6,
        "[generate] (enc_info.ecCodewordsCount / blocks_count) must equal g1g2EcCodewordsCount");
    // NOLINTNEXTLINE
    JLD_QRCODE_ASSERT(enc_info.bitLen == 8 || enc_info.bitLen == 16,
                      QrCodeGenResult::AssertFailure7, "[generate] bitLen must be 8 or 16");
    JLD_QRCODE_ASSERT(enc_info.g1BlocksCount > 0, QrCodeGenResult::AssertFailure8,
                      "[generate] g1BlocksCount must be non-zero");
    JLD_QRCODE_ASSERT(
        (g2_data_codewords_count + enc_info.g1g2EcCodewordsCount) <= m_polyDivData_.data.size(),
        QrCodeGenResult::AssertFailure9, "[generate] g2CodewordsCount exceeds m_polyDivData_ size");
    JLD_QRCODE_ASSERT(enc_info.g1g2EcCodewordsCount < c_maxPolyCoeffsSize,
                      QrCodeGenResult::AssertFailure10,
                      "[generate] g1g2EcCodewordsCount must be smaller than c_maxPolyCoeffsSize");
    JLD_QRCODE_ASSERT(enc_info.g1g2EcCodewordsCount <= c_maxPolyGenSize,
                      QrCodeGenResult::AssertFailure11,
                      "[generate] g1g2EcCodewordsCount exceeds c_maxPolyGenerators");

    if (data.size() > enc_info.writableDataCodewordsCount)
      return QrCodeGenResult::DataTooLarge;

    QrCodeBitLen bit_len = enc_info.bitLen == 8 ? QrCodeBitLen::Bits8 : QrCodeBitLen::Bits16;
    fill_codewords(data, enc_info.dataCodewordsCount, codewords_count, bit_len);

    if constexpr (DebugFlags & QrCodeDebugFlag::c_printRawCodewords_) {
      if constexpr (DebugFlags & QrCodeDebugFlag::c_printInBinary_) {
        std::cout << "[DEBUG] jldQrCodeGen -> Codewords (Binary): [ ";
        for (size_t i = 0; i < enc_info.dataCodewordsCount; ++i)
          std::cout << std::bitset<8>(m_rawCodewords_[i]) << " - ";
        std::cout << " ]\n\n";
      } else {
        std::cout << "[DEBUG] jldQrCodeGen -> Codewords (Decimal): [ ";
        for (size_t i = 0; i < enc_info.dataCodewordsCount; ++i)
          std::cout << static_cast<size_t>(m_rawCodewords_[i]) << " - ";
        std::cout << " ]\n\n";
      }
    }

    interleave_codewords(InterleaveCodewordsParams{
        .g1BlocksCount = enc_info.g1BlocksCount,
        .g2BlocksCount = enc_info.g2BlocksCount,
        .g1DataCodewordsCount = enc_info.g1DataCodewordsCount,
        .g2DataCodewordsCount = g2_data_codewords_count,
        .blocksCount = blocks_count,
    });

    if constexpr (DebugFlags & QrCodeDebugFlag::c_printInterleavedCodewords_) {
      if constexpr (DebugFlags & QrCodeDebugFlag::c_printInBinary_) {
        std::cout << "[DEBUG] jldQrCodeGen -> Interleaved Codewords (Binary): [ ";
        for (size_t i = 0; i < enc_info.dataCodewordsCount; ++i)
          std::cout << std::bitset<8>(m_interleavedCodewords_[i]) << " - ";
        std::cout << " ]\n\n";
      } else {
        std::cout << "[DEBUG] jldQrCodeGen -> Interleaved Codewords (Decimal): [ ";
        for (size_t i = 0; i < enc_info.dataCodewordsCount; ++i)
          std::cout << static_cast<size_t>(m_interleavedCodewords_[i]) << " - ";
        std::cout << " ]\n\n";
      }
    }

    m_polyCoeffs_.data = c_polyGenTable[enc_info.g1g2EcCodewordsCount - 1];
    m_polyCoeffs_.size = enc_info.g1g2EcCodewordsCount + 1;

    generate_ec_data(GenerateEcDataParams{
        .g1BlocksCount = enc_info.g1BlocksCount,
        .g2BlocksCount = enc_info.g2BlocksCount,
        .g1DataCodewordsCount = enc_info.g1DataCodewordsCount,
        .g2DataCodewordsCount = g2_data_codewords_count,
        .g1g2EcCodewordsCount = enc_info.g1g2EcCodewordsCount,
        .blocksCount = blocks_count,
        .dataCodewordsCount = enc_info.dataCodewordsCount,
    });

    if constexpr (DebugFlags & QrCodeDebugFlag::c_printErrorCorrectionData_) {
      if constexpr (DebugFlags & QrCodeDebugFlag::c_printInBinary_) {
        std::cout << "[DEBUG] jldQrCodeGen -> Error-Correction Data (Binary): [ ";
        for (size_t i = enc_info.dataCodewordsCount; i < m_interleavedCodewords_.size; ++i)
          std::cout << std::bitset<8>(m_interleavedCodewords_[i]) << " - ";
        std::cout << " ]\n\n";
      } else {
        std::cout << "[DEBUG] jldQrCodeGen -> Error-Correction Data (Decimal): [ ";
        for (size_t i = enc_info.dataCodewordsCount; i < m_interleavedCodewords_.size; ++i)
          std::cout << static_cast<size_t>(m_interleavedCodewords_[i]) << " - ";
        std::cout << " ]\n\n";
      }
    }

    place_reserved_modules(gen_info.version, enc_info.modulesWidth);

    if constexpr (DebugFlags & QrCodeDebugFlag::c_debugReservedModules_) {
      for (size_t i = 0; i < m_reservedModules_.size(); ++i)
        m_rawModules_[i] = m_reservedModules_[i] == 1 ? 0 : 255;

      if (gen_info.outPath != nullptr)
        return stbi_write_png(gen_info.outPath, static_cast<int>(enc_info.modulesWidth),
                              static_cast<int>(enc_info.modulesWidth), 1, m_rawModules_.data(),
                              0) == 0
                   ? QrCodeGenResult ::StbFailure
                   : QrCodeGenResult::Success;

      return QrCodeGenResult::Success;
    }

    if constexpr (!(DebugFlags & QrCodeDebugFlag::c_disableBitPlacing_))
      place_interleaved_codewords(enc_info.modulesWidth, enc_info.modulesCount);

    if (gen_info.version >= 7) {
      constexpr bool debug_version_info = DebugFlags & QrCodeDebugFlag::c_debugVersionInfo_;
      place_version_info_modules<debug_version_info>(version_idx, enc_info.modulesWidth);
    }

    if constexpr (DebugFlags & QrCodeDebugFlag::c_debugFormatInfo_)
      place_format_info_modules<true, true>(0, enc_info.modulesWidth, QrCodeEcc::M);
    else {
      if constexpr (!(DebugFlags & QrCodeDebugFlag::c_disableMasking_)) {
        if (gen_info.flags & QrCodeGenInfoFlag::c_useCustomMaskIdx_) {
          if (gen_info.customMaskIdx >= 8)
            return QrCodeGenResult::InvalidCustomMaskIdx;

          mask_data_pixels<true>(gen_info.customMaskIdx, enc_info.modulesWidth,
                                 enc_info.modulesCount);
          place_format_info_modules<true, false>(gen_info.customMaskIdx, enc_info.modulesWidth,
                                                 gen_info.ecc);
        } else {
          uint8_t mask_idx =
              apply_best_mask(enc_info.modulesWidth, enc_info.modulesCount, gen_info.ecc);
          if constexpr (DebugFlags & QrCodeDebugFlag::c_printBestMaskIdx_)
            std::cout << "[DEBUG] jldQrCodeGen -> Applied best mask index "
                      << static_cast<size_t>(mask_idx) << "\n";
        }
      }
    }

    if (gen_info.outPath != nullptr) {
      const size_t scale = 4;
      const size_t quiet_zone = 4;
      const size_t quiet_zone_px = quiet_zone * scale;
      const size_t output_width = (enc_info.modulesWidth + quiet_zone * 2) * scale;

      std::vector<uint8_t> out_modules(output_width * output_width, 255);

      for (size_t row = 0; row < enc_info.modulesWidth; ++row) {
        for (size_t y = 0; y < scale; ++y) {
          auto* dst =
              out_modules.data() + (quiet_zone_px + row * scale + y) * output_width + quiet_zone_px;

          for (size_t column = 0; column < enc_info.modulesWidth; ++column) {
            std::fill_n(dst + column * scale, scale,
                        m_rawModules_[row * enc_info.modulesWidth + column] ? 255 : 0);
          }
        }
      }

      return stbi_write_png(gen_info.outPath, static_cast<int>(output_width),
                            static_cast<int>(output_width), 1, out_modules.data(),
                            static_cast<int>(output_width)) == 0
                 ? QrCodeGenResult ::StbFailure
                 : QrCodeGenResult::Success;
    }

    return QrCodeGenResult::Success;
  }

  [[nodiscard]] auto modules() const -> const ModulesArray& {
    return m_rawModules_;
  }

  [[nodiscard]] auto raw_codewords() const -> const CodewordArray& {
    return m_rawCodewords_;
  }

  [[nodiscard]] auto interleaved_codewords() const -> const CodewordArray& {
    return m_interleavedCodewords_;
  }

 private:
  void fill_codewords(const std::vector<uint8_t>& data, const uint32_t data_codewords_count,
                      const uint32_t codewords_count, const QrCodeBitLen bit_len) {
    m_rawCodewords_.size = codewords_count;

    // Byte encoding mode -> 0100
    uint8_t       start{ 0b01000000 };
    const auto    low = static_cast<uint8_t>(data.size());
    const uint8_t high =
        bit_len == QrCodeBitLen::Bits8 ? low : static_cast<uint8_t>(data.size() >> 8);

    // Data size encoding
    start |= high >> 4;
    m_rawCodewords_[0] = start;
    m_rawCodewords_[1] = static_cast<uint8_t>(high << 4);

    size_t codeword_idx{ 1 };
    if (bit_len == QrCodeBitLen::Bits16) {
      codeword_idx = 2;
      m_rawCodewords_[1] |= low >> 4;
      m_rawCodewords_[2] = static_cast<uint8_t>(low << 4);
    }

    // Data encoding
    for (uint8_t b : data) {
      m_rawCodewords_[codeword_idx++] |= b >> 4;
      m_rawCodewords_[codeword_idx] = static_cast<uint8_t>(b << 4);
    }

    // 4 zero bits as termination
    m_rawCodewords_[codeword_idx++] &= 0b11110000;

    // Alternating 11101100 and 00010001 for any unused codeword
    bool alternate{ false };
    while (codeword_idx < data_codewords_count) {
      m_rawCodewords_[codeword_idx++] = alternate ? 0b00010001 : 0b11101100;
      alternate = !alternate;
    }
  }

  struct InterleaveCodewordsParams {
    uint8_t  g1BlocksCount;
    uint8_t  g2BlocksCount;
    uint8_t  g1DataCodewordsCount;
    uint8_t  g2DataCodewordsCount;
    uint32_t blocksCount;
  };

  void interleave_codewords(const InterleaveCodewordsParams& params) {
    m_interleavedCodewords_.size = m_rawCodewords_.size;

    // Since theres only one block, 'm_orderedCodewords_' equals 'm_codewords_', so copy it
    if (params.g1BlocksCount == 1 && params.g2BlocksCount == 0) {
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
    uint32_t interleaved_codewords_idx = 0;
    for (uint8_t i = 0; i < params.g2DataCodewordsCount; ++i) {
      uint32_t raw_codeword_idx = 0;
      for (uint32_t b = 0; b < params.blocksCount; ++b) {
        const bool g1 = b < params.g1BlocksCount;
        // Since 'g2DataCodewordsCount' is always 1 larger than 'g1DataCodewordsCount' and
        // each group 1 block therfore has one codeword less, we have to skip every group 1 block
        // for the g1DataCodewordsCount+1 iteration
        if (g1 && i == params.g1DataCodewordsCount) {
          raw_codeword_idx = params.g1BlocksCount * params.g1DataCodewordsCount;
          b = params.g1BlocksCount - 1;
          continue;
        }

        m_interleavedCodewords_[interleaved_codewords_idx++] =
            m_rawCodewords_[raw_codeword_idx + i];
        raw_codeword_idx += g1 ? params.g1DataCodewordsCount : params.g2DataCodewordsCount;
      }
    }
  }

  struct GenerateEcDataParams {
    uint8_t  g1BlocksCount;
    uint8_t  g2BlocksCount;
    uint8_t  g1DataCodewordsCount;
    uint8_t  g2DataCodewordsCount;
    uint8_t  g1g2EcCodewordsCount;
    uint32_t blocksCount;
    uint32_t dataCodewordsCount;
  };

  void generate_ec_data(const GenerateEcDataParams& params) {
    uint32_t g1_total_codewords_count = params.g1DataCodewordsCount + params.g1g2EcCodewordsCount;
    uint32_t g2_total_codewords_count = g1_total_codewords_count + 1;

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

    uint32_t offset = 0;
    for (uint32_t i = 0; i < params.blocksCount; ++i) {
      const bool g1 = i < params.g1BlocksCount;

      const uint32_t block_data_codewords =
          g1 ? params.g1DataCodewordsCount : params.g2DataCodewordsCount;
      const uint32_t block_total_codewords_count =
          g1 ? g1_total_codewords_count : g2_total_codewords_count;

      set_poly_div_data(block_data_codewords, block_total_codewords_count, offset);
      poly_div(block_data_codewords);

      for (uint32_t c = 0; c < params.g1g2EcCodewordsCount; ++c) {
        // Directly store the error correction data into the interleaved codewords array
        m_interleavedCodewords_[(c * params.blocksCount + i) + params.dataCodewordsCount] =
            m_polyDivData_[c + block_data_codewords];
      }

      offset += block_data_codewords;
    }
  }

  void place_reserved_modules(const uint8_t version, const uint32_t modules_width) {
    // TODO(jld-wk): Memoize, probably? Not sure...

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

    uint32_t module_idx = 0;
    for (uint32_t i = 0; i < 3; ++i) {
      uint32_t                              rows{ static_cast<uint32_t>(i != 2 ? 9 : 8) };
      std::array<std::array<uint8_t, 8>, 8> fp_modules{ fp_modules_to_fill[i] };

      for (uint32_t r = 0; r < rows; ++r) {
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
          for (uint32_t m = 0; m < 8; ++m) m_rawModules_[module_idx + m] = row_modules[m];
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

      // Reserving and placing every alignment patterns: 5x5 each
      uint32_t patterns_per_row{ static_cast<uint32_t>(
          std::floor(static_cast<double>(version) / 7) + 2) };

      uint32_t first_center_column{ 6 };
      uint32_t last_center_column{ modules_width - 7 };

      uint32_t bottom_row{ modules_width - 9 };

      uint32_t odd_dist_between_patterns{ static_cast<uint32_t>(
          std::round((last_center_column - first_center_column) /
                     static_cast<double>((patterns_per_row - 1)))) };
      uint32_t dist_between_patterns{ (odd_dist_between_patterns + 1) -
                                      ((odd_dist_between_patterns + 1) % 2) };

      // I want to iterate from the start and not the center
      last_center_column -= 2;

      for (uint32_t r = 0; r < patterns_per_row; ++r) {
        uint32_t last_pattern{ patterns_per_row - 1 };
        uint32_t row{ r == last_pattern ? 4 : bottom_row - (dist_between_patterns * r) };

        bool is_first_row{ r == 0 };
        bool is_last_row{ r == last_pattern };

        uint32_t row_module_idx{ row * modules_width };

        for (uint32_t c = 0; c < patterns_per_row; ++c) {
          // Skip finder patterns
          if ((c == last_pattern && (is_last_row || is_first_row)) || (c == 0 && is_last_row))
            continue;

          uint32_t column{ c == 0              ? last_center_column
                           : c == last_pattern ? 4
                                               : last_center_column - (dist_between_patterns * c) };

          uint32_t module_idx{ row_module_idx + column };
          for (uint32_t pr = 0; pr < 5; ++pr) {
            m_reservedModules_[module_idx] = 1;
            m_reservedModules_[module_idx + 1] = 1;
            m_reservedModules_[module_idx + 2] = 1;
            m_reservedModules_[module_idx + 3] = 1;
            m_reservedModules_[module_idx + 4] = 1;

            std::array<uint8_t, 5> to_fill{ ap_modules_to_fill[pr] };
            for (uint32_t m = 0; m < 5; ++m) m_rawModules_[module_idx + m] = to_fill[m];
            module_idx += modules_width;
          }
        }
      }
    }

    uint8_t version_mul4{ static_cast<uint8_t>(version * 4) };

    // Reserving timing patterns
    reserve_area(ReserveAreaParams{
        .row = 6, .column = 9, .width = version_mul4, .height = 1, .modulesWidth = modules_width });
    reserve_area(ReserveAreaParams{
        .row = 9, .column = 6, .width = 1, .height = version_mul4, .modulesWidth = modules_width });

    // Placing the timing patterns (horizontally and vertically). They alternate between dark and
    // light modules
    uint32_t tp_end{ static_cast<uint32_t>(2 + version_mul4) };

    uint32_t h_idx{ (6 * modules_width) + 6 };
    uint32_t v_idx{ (6 * modules_width) + 6 };
    uint32_t h_stride{ modules_width * 2 };
    uint32_t v_stride{ 2 };

    for (uint32_t i = 0; i < tp_end; i += 2) {
      m_rawModules_[v_idx += v_stride] = 0;
      m_rawModules_[h_idx += h_stride] = 0;
    }

    // Version 7 and below don't have version information
    if (version >= 7) {
      // Reserving version information modules
      reserve_area(ReserveAreaParams{ .row = modules_width - 11,
                                      .column = 0,
                                      .width = 6,
                                      .height = 3,
                                      .modulesWidth = modules_width });
      reserve_area(ReserveAreaParams{ .row = 0,
                                      .column = modules_width - 11,
                                      .width = 3,
                                      .height = 6,
                                      .modulesWidth = modules_width });
    }

    // Reserving dark module
    module_idx = ((modules_width - 8) * modules_width) + 8;
    m_rawModules_[module_idx] = 0;
    m_reservedModules_[module_idx] = 1;
  }

  struct ReserveAreaParams {
    uint32_t row;
    uint32_t column;
    uint32_t width;
    uint32_t height;
    uint32_t modulesWidth;
  };

  void reserve_area(const ReserveAreaParams& params) {
    for (uint32_t r = params.row; r < params.row + params.height; ++r) {
      uint32_t row_module_idx = r * params.modulesWidth;
      for (uint32_t c = params.column; c < params.column + params.width; ++c)
        m_reservedModules_[row_module_idx + c] = 1;
    }
  }

  void place_interleaved_codewords(const uint32_t modules_width, const uint32_t modules_count) {
    class CodewordBitReader {
     public:
      explicit CodewordBitReader(const CodewordArray& codewords)
          : m_codewords_{ codewords } {}
      ~CodewordBitReader() = default;

      CodewordBitReader(const CodewordBitReader&) = delete;
      CodewordBitReader(CodewordBitReader&&) = delete;
      auto operator=(const CodewordBitReader&) -> CodewordBitReader& = delete;
      auto operator=(CodewordBitReader&&) -> CodewordBitReader& = delete;

      [[nodiscard]] auto next() -> uint8_t {
        const uint8_t idx = m_bitIdx_--;
        const uint8_t bit = m_codewordBits_[idx];

        if (idx == 0) {
          ++m_wordIdx_;
          m_bitIdx_ = 7;
          if (m_wordIdx_ < m_codewords_.size)
            m_codewordBits_ = m_codewords_[m_wordIdx_];
        }

        return bit == 1 ? 0 : 255;
      }

      [[nodiscard]] auto in_bounds() const -> bool {
        return m_wordIdx_ != m_codewords_.size;
      }

     private:
      uint8_t  m_bitIdx_ = 7;
      uint32_t m_wordIdx_ = 0;

      const CodewordArray& m_codewords_;
      std::bitset<8>       m_codewordBits_{ m_codewords_[0] };
    };

    uint32_t          module_idx{ modules_count };
    CodewordBitReader bits{ m_interleavedCodewords_ };

    m_rawModules_[--module_idx] = bits.next();
    m_rawModules_[--module_idx] = bits.next();

    bool going_up = true;
    bool may_move_left = false;

    uint32_t last_row_module_idx = modules_count - modules_width;

    // TODO(jld-wk): Add a description of the algorithm

    auto check_row_availability = [&]() -> bool {
      if (going_up) {
        for (uint32_t i = module_idx; i >= modules_width;) {
          i -= modules_width;

          if (!m_reservedModules_[i]) {
            module_idx = i + modules_width;
            return true;
          }

          if (i < modules_width) {
            if (!m_reservedModules_[i - 2]) {
              module_idx = i - 2;
              m_rawModules_[module_idx] = bits.next();

              going_up = false;
              return true;
            }
          }
        }
      } else {
        for (uint32_t i = module_idx + modules_width; i < modules_count; i += modules_width) {
          if (!m_reservedModules_[i]) {
            module_idx = i - modules_width;
            return true;
          }

          if (i >= last_row_module_idx) {
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

      uint32_t next_module_idx =
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
          check_row_availability();
          m_rawModules_[module_idx -= modules_width] = bits.next();
          continue;
        }
      }

      going_up = !going_up;
    }
  }

  template <bool WriteRaw, bool Debug>
  void place_format_info_modules(const uint8_t mask_idx, const uint32_t modules_width,
                                 const QrCodeEcc ecc) {
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
        c_formatInfoTable[Debug ? 4 : static_cast<uint8_t>(ecc)][Debug ? 0 : mask_idx];

    uint32_t module_idx = (8 * modules_width);

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

  template <bool Debug>
  void place_version_info_modules(const uint8_t version_idx, const uint32_t modules_width) {
    std::array<uint8_t, 18> version_info{ c_versionInfoTable[Debug ? 0 : version_idx] };

    // TODO(jld-wk): Add a nice graphic like in info module, I should become a graphics designer.

    uint32_t module_idx{ (modules_width - 11) * modules_width };

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

  auto apply_best_mask(const uint32_t modules_width, const uint32_t modules_count,
                       const QrCodeEcc ecc) -> uint8_t {
    uint8_t  best_mask = 0;
    uint32_t lowest_penalty = UINT32_MAX;

    for (uint8_t i = 0; i < 8; ++i) {
      mask_data_pixels<false>(i, modules_width, modules_count);
      place_format_info_modules<false, false>(i, modules_width, ecc);

      uint32_t penalty = compute_mask_penalty(modules_width, modules_count);
      if (penalty < lowest_penalty) {
        best_mask = i;
        lowest_penalty = penalty;
      }
    }

    mask_data_pixels<true>(best_mask, modules_width, modules_count);
    place_format_info_modules<true, false>(best_mask, modules_width, ecc);

    return best_mask;
  }

  template <bool WriteRaw>
  void mask_data_pixels(const uint8_t mask_idx, const uint32_t modules_width,
                        const uint32_t modules_count) {
    /*
      Simply performs a mask function on every module which is not reserved (data and error
      correction data). WriteRaw is used to avoid copying from 'm_maskedModules_' to
      'm_rawModules' once the best mask index is choosen.
    */

    const auto mask_fn = c_maskFns_[mask_idx];

    uint32_t row = 0;
    uint32_t column = 0;

    uint32_t row_module_idx{ modules_width };

    for (uint32_t i = 0; i < modules_count; ++i, ++column) {
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

  // TODO(jld-wk): It's not correct, its off by a bit, but it's so tedious to test, I don't
  // really care. But sometime?
  [[nodiscard]] auto compute_mask_penalty(const uint32_t modules_width,
                                          const uint32_t modules_count) -> uint32_t {
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

    uint32_t penalty_score{ 0 };

    uint32_t sequence_len{ 0 };
    uint8_t  counting_val{ 0 };

    // Lambdas seem to be faster than the class one with 2 less instructions
    auto try_apply_rule1 = [&](const uint8_t module, const bool new_row) -> void {
      if (module != counting_val || new_row) {
        counting_val = module;
        sequence_len = 1;
        return;
      }

      ++sequence_len;
      penalty_score += sequence_len == 5 ? 3 : sequence_len > 5 ? 1 : 0;
    };

    auto try_apply_rule3 = [=, &penalty_score, this]<bool ApplyForColumn>(
                               uint32_t module_idx, const uint32_t row_module_idx,
                               const uint8_t module) -> void {
      if (ApplyForColumn ? ((module_idx + 11) > row_module_idx)
                         : ((module_idx * 11) >= modules_count))
        return;

      const size_t stride = ApplyForColumn ? 1 : modules_width;

      uint8_t module_0 = module;
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
          penalty_score += 40;
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
          penalty_score += 40;
      }
    };

    bool     new_row{ false };
    uint32_t row_module_idx{ modules_width };

    uint32_t dark_modules_count{ 0 };

    for (uint32_t i = 0; i < modules_count; ++i) {
      uint8_t module = m_maskedModules_[i];
      if (module == 1)
        ++dark_modules_count;

      try_apply_rule1(module, new_row);
      new_row = false;

      // wow, cool, c++ syntax!
      try_apply_rule3.template operator()<true>(i, row_module_idx, module);

      if (i >= (row_module_idx - 1)) {
        new_row = true;
        row_module_idx += modules_width;
      }

      size_t bottom_right_idx{ i + modules_width + 1 };
      if (bottom_right_idx < modules_count && !new_row) {
        if (m_maskedModules_[i + 1] == module && m_maskedModules_[i + modules_width] == module &&
            m_maskedModules_[bottom_right_idx] == module)
          penalty_score += 3;
      }
    }

    sequence_len = 0;
    counting_val = 0;

    for (uint32_t c = 0; c < modules_width; ++c) {
      uint32_t module_idx{ c };
      for (uint32_t r = 0; r < modules_width; ++r) {
        uint8_t module = m_maskedModules_[module_idx];
        try_apply_rule1(module, false);
        try_apply_rule3.template operator()<true>(module_idx, 0, module);
        module_idx += modules_width;
      }
    }

    uint32_t dark_modules_percentage{ static_cast<uint32_t>(
        std::floor((dark_modules_count / static_cast<double>(modules_count)) * 100)) };

    dark_modules_percentage -= dark_modules_percentage > 50
                                   ? (dark_modules_percentage % 5)
                                   : ((dark_modules_percentage + 4) % 5) - 4;
    penalty_score +=
        static_cast<uint32_t>(std::abs(static_cast<int32_t>(50 - dark_modules_percentage)) * 2);
    return penalty_score;
  }

  [[nodiscard]] auto gf256_mul(uint8_t a, uint8_t b) const -> uint8_t {
    // A special way of multiplication which basically maps the result of A * B to something
    // between 0 and 255, which is required for further operations on the product of large values
    // as a byte
    return c_expTable[(c_logTable[a] + c_logTable[b]) % 255];
  }

  void poly_div(const uint32_t block_data_codewords) {
    /*
      Polynomial division using GF(256) multiplication.

      Iterates through 'm_polyDivData_' which must be set before using
      'set_polynomial_division_data' and check if it's non-zero, otherwise it just continues,
      let's say that's X and then performs for the next N (m_polyCoeffs_.size) bytes (including X)
      a XOR with the multiplication of X and the current coefficient (m_polyCoeffs_, 0-N) in
      GF(256)
    */
    for (uint32_t i = 0; i < block_data_codewords; ++i) {
      uint8_t start{ m_polyDivData_[i] };
      if (start != 0) {
        for (size_t g = 0; g < m_polyCoeffs_.size; ++g)
          m_polyDivData_[i + g] ^= gf256_mul(start, m_polyCoeffs_[g]);
      }
    }
  }

  void set_poly_div_data(const uint32_t block_data_codewords, const uint32_t block_total_codewords,
                         const uint32_t offset) {
    for (uint32_t i = 0; i < block_total_codewords; ++i)
      m_polyDivData_[i] = (i >= block_data_codewords ? 0 : m_rawCodewords_[i + offset]);
    m_polyDivData_.size = block_total_codewords;
  }

 private:
  static constexpr std::array<bool (*)(size_t, size_t), 8> c_maskFns_ = {
    [](size_t row, size_t column) -> bool { return ((row + column) % 2) == 0; },
    [](size_t row, size_t) -> bool { return (row % 2) == 0; },
    [](size_t, size_t column) -> bool { return (column % 3) == 0; },
    [](size_t row, size_t column) -> bool { return ((row + column) % 3) == 0; },
    [](size_t row, size_t column) -> bool {
      return (static_cast<size_t>(std::floor(row / 2) + std::floor(column / 3)) % 2) == 0;
    },
    [](size_t row, size_t column) -> bool { return (row * column % 2 + row * column % 3) == 0; },
    [](size_t row, size_t column) -> bool {
      return (((row * column) % 2 + (row * column) % 3) % 2) == 0;
    },
    [](size_t row, size_t column) -> bool {
      return (((row + column) % 2 + (row * column) % 3) % 2) == 0;
    }
  };

  CodewordArray m_rawCodewords_;
  CodewordArray m_interleavedCodewords_;

  ModulesArray                   m_rawModules_{};
  ModulesArray                   m_maskedModules_{};
  std::bitset<c_maxModulesCount> m_reservedModules_;

  ByteArray<c_maxPolyCoeffsSize>  m_polyCoeffs_;
  ByteArray<c_maxPolyDivDataSize> m_polyDivData_;
};

}  // namespace jld

#endif  // JLD_QRCODE_HPP