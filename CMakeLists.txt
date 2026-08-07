cmake_minimum_required(VERSION 3.20 FATAL_ERROR)
message("Configuring: ${CMAKE_CURRENT_SOURCE_DIR}")

set(CMAKE_PROJECT_VERSION 0.0.5)

project(avm
    VERSION ${CMAKE_PROJECT_VERSION}
    DESCRIPTION "Associative virtual machine"
    LANGUAGES CXX)

enable_testing()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(AVM_BUILD_LEGACY "Build the legacy JSON compatibility executable and tests" ON)
option(AVM_BUILD_CORE_TESTS "Build AVM 1.0 core tests" ON)
option(AVM_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
option(AVM_ENABLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)

function(avm_apply_quality target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4)
        if(AVM_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE /WX)
        endif()
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
        if(AVM_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE -Werror)
        endif()

        if(AVM_ENABLE_SANITIZERS)
            target_compile_options(${target} PRIVATE
                -fsanitize=address,undefined
                -fno-omit-frame-pointer)
            target_link_options(${target} PRIVATE
                -fsanitize=address,undefined)
        endif()
    endif()
endfunction()

if(AVM_BUILD_LEGACY)
    set(TARGET_NAME avm)

    add_executable(${TARGET_NAME})

    target_include_directories(${TARGET_NAME} PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/3p")

    target_sources(${TARGET_NAME} PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/include/avm.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/3p/UnitedMemoryLinks.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp")

    if(WIN32)
        add_definitions(-DUNICODE -D_UNICODE)
        link_directories("${CMAKE_CURRENT_SOURCE_DIR}/3p")
        target_link_libraries(${PROJECT_NAME} PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/3p/doublets_ffi.dll.lib")

        if(MSVC)
            target_link_options(${TARGET_NAME} PRIVATE /STACK:8388608)
        else()
            target_link_options(${TARGET_NAME} PRIVATE -Wl,--stack,8388608)
        endif()
    endif()

    set(TEST_DIR "${CMAKE_CURRENT_SOURCE_DIR}/test")
    set(TEST_FILES
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
        root.json)

    add_executable(avm_unit_test
        "${CMAKE_CURRENT_SOURCE_DIR}/test/unit_test.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/include/avm.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/3p/UnitedMemoryLinks.h")

    target_include_directories(avm_unit_test PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/3p")

    target_compile_definitions(avm_unit_test PRIVATE AVM_NO_MAIN)

    if(WIN32)
        target_link_libraries(avm_unit_test PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/3p/doublets_ffi.dll.lib")
        if(MSVC)
            target_link_options(avm_unit_test PRIVATE /STACK:8388608)
        else()
            target_link_options(avm_unit_test PRIVATE -Wl,--stack,8388608)
        endif()
    endif()

    add_test(NAME unit_tests COMMAND avm_unit_test)

    foreach(TEST_FILE ${TEST_FILES})
        add_test(
            NAME "json_roundtrip_${TEST_FILE}"
            COMMAND ${CMAKE_COMMAND}
                -DAVM_EXECUTABLE=$<TARGET_FILE:${TARGET_NAME}>
                -DTEST_FILE=${TEST_DIR}/${TEST_FILE}
                -DTEST_DIR=${TEST_DIR}
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/run_test.cmake")
    endforeach()
endif()

if(AVM_BUILD_CORE_TESTS)
    function(avm_add_core_test target source test_name)
        add_executable(${target} "${CMAKE_CURRENT_SOURCE_DIR}/${source}")
        target_include_directories(${target} PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/include")
        avm_apply_quality(${target})
        add_test(NAME ${test_name} COMMAND ${target})
    endfunction()

    avm_add_core_test(
        avm_link_store_test
        test/link_store_test.cpp
        link_store_tests)

    avm_add_core_test(
        avm_relations_model_test
        test/relations_model_test.cpp
        relations_model_tests)

    avm_add_core_test(
        avm_executor_test
        test/executor_test.cpp
        executor_tests)

    avm_add_core_test(
        avm_program_model_test
        test/program_model_test.cpp
        program_model_tests)

    avm_add_core_test(
        avm_boolean_runtime_test
        test/boolean_runtime_test.cpp
        boolean_runtime_tests)

    add_custom_target(avm_core_tests DEPENDS
        avm_link_store_test
        avm_relations_model_test
        avm_executor_test
        avm_program_model_test
        avm_boolean_runtime_test)
endif()
