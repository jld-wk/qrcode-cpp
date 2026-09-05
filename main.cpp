#include <chrono>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <vector>

#include "qrcode.hpp"

int main() {
  std::fstream f{ "data.txt", std::ios::in | std::ios::binary };
  if (!f.is_open()) {
    std::cerr << "Couldn't open file: data.txt\n";
    return 1;
  }

  f.seekg(0, std::ios::end);
  size_t data_size = static_cast<size_t>(f.tellg());

  if (data_size == 0) {
    std::cerr << "Nothing to encode: data.txt\n";
    return 1;
  }

  f.seekg(0, std::ios::beg);

  std::vector<uint8_t> data(data_size);
  f.read(reinterpret_cast<char*>(data.data()), data.size());

  auto start = std::chrono::high_resolution_clock::now();

  QrCode qr_code{ data, 4, Ecc::L, false };

  auto end = std::chrono::high_resolution_clock::now();
  auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

  std::cout << "Generated the QR-Code in " << diff.count() << "ms!\n";

  return 0;
}