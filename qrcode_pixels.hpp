#ifndef QRCODE_PIXELS_HPP
#define QRCODE_PIXELS_HPP

#include <cstdint>
#include <vector>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

// TODO: reserved pixles safety checks

class QrCodePixels {
 public:
  QrCodePixels(size_t wh)
      : m_wh_(wh)
      , m_pixels_(wh * wh, 255)
      , m_reserved_(wh * wh, false) {}

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
                 const std::vector<uint8_t>& data, size_t offset = 0, bool debug = false) {
    size_t i = offset;
    for (size_t r = m_wh_ - row; r-- != m_wh_ - row - height;) {
      for (size_t c = column; c < column + width; ++c) {
        size_t idx = pixel_idx(r, c);
        m_pixels_[idx] = debug ? data[i] : data[i] == 1 ? 0 : 255;
        ++i;
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

 private:
  size_t               m_wh_ = 0;
  std::vector<uint8_t> m_pixels_;
  std::vector<bool>    m_reserved_;
};

#endif