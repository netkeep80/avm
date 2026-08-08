cmake_minimum_required(VERSION 3.20 FATAL_ERROR)

project(avm_package_consumer LANGUAGES CXX)

enable_testing()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(avm 1.3 CONFIG REQUIRED)

add_executable(avm_package_consumer main.cpp)
target_link_libraries(avm_package_consumer PRIVATE avm::core)

if(MSVC)
    target_compile_options(avm_package_consumer PRIVATE /W4 /WX)
else()
    target_compile_options(avm_package_consumer PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

add_test(NAME installed_package_consumer COMMAND avm_package_consumer)
