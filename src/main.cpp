#include <cstddef>
#include <cstdint>
#include <fstream>
#include <ios>
#include <iostream>
#include <vector>

#include "qrcode/core.hpp"

auto main() -> int {
  std::fstream f{ "example-data.txt", std::ios::in | std::ios::binary };
  if (!f.is_open()) {
    std::cerr << "Couldn't open file: example-data.txt\n";
    return 1;
  }

  f.seekg(0, std::ios::end);
  size_t data_size = static_cast<size_t>(f.tellg());

  if (data_size == 0) {
    std::cerr << "Nothing to encode: example-data.txt\n";
    return 1;
  }

  f.seekg(0, std::ios::beg);

  std::vector<uint8_t> data(data_size);
  f.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(data.size()));

  jld::QrCodeGenInfo info{
    .ecc = jld::QrCodeEcc::L,
    .version = 40,
    .outPath = "example-out.png",
    .customMaskIdx = 0,  // It's unused
    .flags = jld::QrCodeGenInfoFlag::c_none_,
  };

  jld::QrCodeGenerator generator;
  jld::QrCodeGenResult result = generator.generate<jld::QrCodeDebugFlag::c_none_>(data, info);

  if (result != jld::QrCodeGenResult::Success) {
    std::cerr << "Failed to generate!\n";
    return 1;
  }

  return 0;
}