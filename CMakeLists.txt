cmake_minimum_required(VERSION 3.16)
project(bishe VERSION 0.1.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
find_package(OpenSSL REQUIRED)
# set(sources )
add_library(xdelta3 ../xdelta3/xdelta3.c)
add_executable(deduplication readFile.cc)
target_link_libraries(deduplication OpenSSL::SSL OpenSSL::Crypto)
target_link_libraries(deduplication xdelta3)
enable_testing()
# target_include_directories(deduplication PUBLIC .)
