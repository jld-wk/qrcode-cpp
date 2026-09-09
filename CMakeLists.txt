cmake_minimum_required(VERSION 3.10.0)
project(qrcode VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenCV REQUIRED)

add_executable(qrcode src/main.cpp)

target_link_libraries(qrcode
    PRIVATE
    ${OpenCV_LIBS}
)