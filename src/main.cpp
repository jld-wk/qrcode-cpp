#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <vector>

#include "qrcode.hpp"
#include "qrcode_memoizer.hpp"

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

  /*
  QrCodeMemoizer memoizer{ "src/qrcode_memoized.hpp" };
  memoizer.generate_encoding_info();
  memoizer.generate_exp_table();
  memoizer.generate_log_table();
  memoizer.generate_polynomial_generator();
  memoizer.generate_format_information();
  memoizer.generate_version_information();*/

  auto start = std::chrono::high_resolution_clock::now();

  QrCodeGenerator generator;

  constexpr QrCodeDebugFlag debug_flags = QrCodeDebugFlag::None;

  constexpr QrCodeInfo info{
    .ecc = Ecc::L,
    .version = 40,
  };
  constexpr QrCodeGenerationInfo gen_info = generator.gen_info<info>();

  generator.generate(data, info, debug_flags);

  auto end = std::chrono::high_resolution_clock::now();
  auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

  std::cout << "Generated the QR-Code in " << diff.count() << "ms!\n";

  return 0;
}