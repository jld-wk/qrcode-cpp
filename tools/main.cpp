#include <iostream>

#include "memoizer.hpp"

auto main(int argc, const char** argv) -> int {
  const char* out_filepath = "./memoized.hpp";
  if (argc > 1)
    out_filepath = argv[1];

  jld::QrCodeMemoizer memoizer{ out_filepath };
  if (memoizer.successful())
    std::cout << "-> Tools ran successfully!\n";
}