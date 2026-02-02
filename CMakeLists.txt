# Require CMAKE 3.20 or higher
cmake_minimum_required(VERSION 3.20 FATAL_ERROR)
message( "Configuring: ${CMAKE_CURRENT_SOURCE_DIR}")

set(CMAKE_PROJECT_VERSION 0.0.1 )

# Project name
project( avm
         VERSION ${CMAKE_PROJECT_VERSION}
         DESCRIPTION "Associative virtual machine"
         LANGUAGES CXX )

enable_testing()

# name template
set( TARGET_NAME avm )

# Build for C++17 if we can
set( CMAKE_CXX_STANDARD 20 )
set( CMAKE_CXX_STANDARD_REQUIRED ON )
set( CMAKE_CXX_EXTENSIONS OFF )

# Name of executable
add_executable( ${TARGET_NAME} )

# LinksPlatform dependency (Windows only)
if( WIN32 )
    link_directories(
        "${CMAKE_CURRENT_SOURCE_DIR}/3p" )

    target_link_libraries(${PROJECT_NAME} PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/3p/doublets_ffi.dll.lib" )
endif()

# Adding build-requirements
target_include_directories( ${TARGET_NAME} PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/3p" )

target_sources( ${TARGET_NAME} PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include/avm.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/json_io.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/anumber.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/3p/UnitedMemoryLinks.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp" )

if( WIN32 )
    add_definitions(-DUNICODE -D_UNICODE)
endif()

# Test target for anumber serialization/deserialization
add_executable( test_anumber
    "${CMAKE_CURRENT_SOURCE_DIR}/test/src/test_anumber.cpp" )

target_include_directories( test_anumber PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/3p" )

if( WIN32 )
    target_link_libraries( test_anumber PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/3p/doublets_ffi.dll.lib" )
endif()

add_test( NAME test_anumber COMMAND test_anumber )
