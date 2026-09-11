cmake_minimum_required(VERSION 3.10.0)
project(qrcode_gen VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# find_package(OpenCV REQUIRED)

add_executable(qrcode_gen src/main.cpp)

include_directories(qrcode_gen PRIVATE "src")
include_directories(qrcode_gen PRIVATE SYSTEM "vendor")

target_compile_options(qrcode_gen PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Wconversion
    -Werror
    -Wnrvo
)
# target_link_libraries(qrcode_gen
#    PRIVATE
#    ${OpenCV_LIBS}
#)