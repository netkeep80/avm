cmake_minimum_required(VERSION 3.20 FATAL_ERROR)

project(avm_benchmark LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(avm_benchmark avm_benchmark.cpp)
target_include_directories(avm_benchmark PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../include")

if(MSVC)
    target_compile_options(avm_benchmark PRIVATE /W4 /WX)
else()
    target_compile_options(avm_benchmark PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
