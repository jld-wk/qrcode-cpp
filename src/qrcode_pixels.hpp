#ifndef QRCODE_PIXELS_HPP
#define QRCODE_PIXELS_HPP

#include <array>
#include <cstdint>
#include <vector>

#include "qrcode_arrays.hpp"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

// TODO: reserved pixles safety checks

class QrCodePixels {
 public:
  void fill_area(size_t row, size_t column, size_t width, size_t height, bool fill = true,
                 bool reserve = false) {
    for (size_t r = row; r < row + height; ++r) {
      for (size_t c = column; c < column + width; ++c) {
        size_t idx = pixel_idx(r, c);
        m_pixels_[idx] = static_cast<uint8_t>(fill ? 0 : 255);
        m_reserved_[idx] = reserve;
      }
    }
  }

  void fill_area(size_t row, size_t column, size_t width, size_t height,
                 const std::array<uint8_t, 15>& data, size_t offset = 0, bool debug = false,
                 bool fill_inverted = false) {
    size_t i = fill_inverted ? (offset + (width > 1 ? width : height) - 1) : offset;
    for (size_t r = m_wh_ - row; r-- != m_wh_ - row - height;) {
      for (size_t c = column; c < column + width; ++c) {
        size_t idx = pixel_idx(r, c);
        m_pixels_[idx] = debug ? data[i] : data[i] == 1 ? 0 : 255;
        m_reserved_[idx] = true;
        fill_inverted ? --i : ++i;
      }
    }
  }

  void fill_area(size_t row, size_t column, size_t width, size_t height,
                 const std::array<uint8_t, 18>& data, size_t offset = 0, bool debug = false,
                 bool fill_inverted = false) {
    size_t i = fill_inverted ? (offset + (width > 1 ? width : height) - 1) : offset;
    for (size_t r = m_wh_ - row; r-- != m_wh_ - row - height;) {
      for (size_t c = column; c < column + width; ++c) {
        size_t idx = pixel_idx(r, c);
        m_pixels_[idx] = debug ? data[i] : data[i] == 1 ? 0 : 255;
        m_reserved_[idx] = true;
        fill_inverted ? --i : ++i;
      }
    }
  }

  bool is_reserved(size_t row, size_t column) {
    return m_reserved_[pixel_idx(row, column)];
  }

  uint8_t pixel(size_t row, size_t column) {
    return m_pixels_[pixel_idx(row, column)];
  }

  std::vector<uint8_t>& pixels() {
    return m_pixels_;
  }

  void fill_pixel(size_t row, size_t column, bool fill = true, bool reserve = false) {
    size_t idx = pixel_idx(row, column);
    m_pixels_[idx] = static_cast<uint8_t>(fill ? 0 : 255);
    m_reserved_[idx] = reserve;
  }

  void write_jpg() {
    stbi_write_jpg("out.jpg", m_wh_, m_wh_, 1, m_pixels_.data(), 100);
  }

  size_t pixel_idx(size_t row, size_t column) {
    /*size_t idx = (m_wh_ * row) + column;
    if (idx >= m_pixels_.size())
      return 0;*/
    return (m_wh_ * row) + column;
  }

  void resize(size_t modules_width, size_t total_modules) {
    m_wh_ = modules_width;
    m_pixels_.resize(total_modules, 255);
    m_reserved_.resize(total_modules, false);
  }

 private:
  size_t               m_wh_ = 0;
  std::vector<uint8_t> m_pixels_;
  std::vector<bool>    m_reserved_;
};

#endif