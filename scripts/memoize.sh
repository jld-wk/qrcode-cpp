#!/usr/bin/env bash

cd "$(dirname "$0")"

mkdir ../build
cd ../build
g++ -O3 -std=c++23 -Wpedantic -Wextra -Wall -Werror ../tools/main.cpp -o ../build/run_tools -I../src
./run_tools ../src/qrcode/memoized.hpp
clang-format ../src/qrcode/memoized.hpp -i --style=file