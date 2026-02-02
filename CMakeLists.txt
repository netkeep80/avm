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

# Build for C++20
set( CMAKE_CXX_STANDARD 20 )
set( CMAKE_CXX_STANDARD_REQUIRED ON )
set( CMAKE_CXX_EXTENSIONS OFF )

# Name of executable
add_executable( ${TARGET_NAME} )

# Adding build-requirements
target_include_directories( ${TARGET_NAME} PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/3p" )

target_sources( ${TARGET_NAME} PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include/avm.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/3p/UnitedMemoryLinks.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp" )

if( WIN32 )
    add_definitions(-DUNICODE -D_UNICODE)
    link_directories(
        "${CMAKE_CURRENT_SOURCE_DIR}/3p" )
    target_link_libraries(${PROJECT_NAME} PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/3p/doublets_ffi.dll.lib" )
endif()

# Add tests: for each test JSON file, run avm and compare output with expected
set( TEST_DIR "${CMAKE_CURRENT_SOURCE_DIR}/test" )
set( TEST_FILES
    true.json
    false.json
    null.json
    "[true].json"
    "[false].json"
    "[null].json"
    array.json
    number.json
    unsigned.json
    text.json
    text2.json
    strings.json
    nulls.json
    empty_array.json
    empty_object.json
    root.json
)

foreach( TEST_FILE ${TEST_FILES} )
    add_test(
        NAME "json_roundtrip_${TEST_FILE}"
        COMMAND ${CMAKE_COMMAND}
            -DAVM_EXECUTABLE=$<TARGET_FILE:${TARGET_NAME}>
            -DTEST_FILE=${TEST_DIR}/${TEST_FILE}
            -DTEST_DIR=${TEST_DIR}
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/run_test.cmake"
    )
endforeach()
