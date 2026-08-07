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

option(AVM_BUILD_LEGACY "Build the legacy JSON codec/facade executable and tests" ON)
option(AVM_BUILD_CORE_TESTS "Build AVM 1.0 core tests" ON)
option(AVM_BUILD_JSON_COMPAT_TESTS "Build JSON projection tests" ON)
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

function(avm_enable_test_assertions target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /UNDEBUG)
    else()
        target_compile_options(${target} PRIVATE -UNDEBUG)
    endif()
endfunction()

function(avm_link_legacy_windows target)
    if(WIN32)
        target_link_libraries(${target} PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/3p/doublets_ffi.dll.lib")
        if(MSVC)
            target_link_options(${target} PRIVATE /STACK:8388608)
        else()
            target_link_options(${target} PRIVATE -Wl,--stack,8388608)
        endif()
    endif()
endfunction()

if(AVM_BUILD_LEGACY)
    set(TARGET_NAME avm)

    if(WIN32)
        add_definitions(-DUNICODE -D_UNICODE)
        link_directories("${CMAKE_CURRENT_SOURCE_DIR}/3p")
    endif()

    add_executable(${TARGET_NAME}
        "${CMAKE_CURRENT_SOURCE_DIR}/src/cli.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/legacy_json_compat.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/include/avm.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/include/avm/legacy_json_compat.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/3p/UnitedMemoryLinks.h")
    target_include_directories(${TARGET_NAME} PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/3p")
    avm_apply_quality(${TARGET_NAME})
    avm_link_legacy_windows(${TARGET_NAME})

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
        "${CMAKE_CURRENT_SOURCE_DIR}/src/legacy_json_compat.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/include/avm.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/include/avm/legacy_json_compat.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/3p/UnitedMemoryLinks.h")
    target_include_directories(avm_unit_test PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/3p")
    avm_enable_test_assertions(avm_unit_test)
    avm_link_legacy_windows(avm_unit_test)
    add_test(NAME unit_tests COMMAND avm_unit_test)

    add_executable(avm_legacy_facade_test
        "${CMAKE_CURRENT_SOURCE_DIR}/test/legacy_facade_test.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/legacy_json_compat.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/include/avm.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/include/avm/legacy_json_compat.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/3p/UnitedMemoryLinks.h")
    target_include_directories(avm_legacy_facade_test PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/3p")
    avm_apply_quality(avm_legacy_facade_test)
    avm_enable_test_assertions(avm_legacy_facade_test)
    avm_link_legacy_windows(avm_legacy_facade_test)
    add_test(NAME legacy_facade_tests COMMAND avm_legacy_facade_test)

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
        avm_enable_test_assertions(${target})
        add_test(NAME ${test_name} COMMAND ${target})
    endfunction()

    avm_add_core_test(
        avm_assertions_enabled_test
        test/assertions_enabled_test.cpp
        assertions_enabled_tests)

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

    avm_add_core_test(
        avm_function_runtime_test
        test/function_runtime_test.cpp
        function_runtime_tests)

    avm_add_core_test(
        avm_frame_runtime_test
        test/frame_runtime_test.cpp
        frame_runtime_tests)

    avm_add_core_test(
        avm_deferred_definition_test
        test/deferred_definition_test.cpp
        deferred_definition_tests)

    avm_add_core_test(
        avm_projection_test
        test/projection_test.cpp
        projection_tests)

    avm_add_core_test(
        avm_raw_carrier_test
        test/raw_carrier_test.cpp
        raw_carrier_tests)

    add_custom_target(avm_core_tests DEPENDS
        avm_assertions_enabled_test
        avm_link_store_test
        avm_relations_model_test
        avm_executor_test
        avm_program_model_test
        avm_boolean_runtime_test
        avm_function_runtime_test
        avm_frame_runtime_test
        avm_deferred_definition_test
        avm_projection_test
        avm_raw_carrier_test)
endif()

if(AVM_BUILD_JSON_COMPAT_TESTS)
    add_executable(avm_json_projection_test
        "${CMAKE_CURRENT_SOURCE_DIR}/test/json_projection_test.cpp")
    target_include_directories(avm_json_projection_test PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/3p")
    avm_apply_quality(avm_json_projection_test)
    avm_enable_test_assertions(avm_json_projection_test)
    add_test(NAME json_projection_tests COMMAND avm_json_projection_test)

    add_custom_target(avm_json_compat_tests DEPENDS avm_json_projection_test)
endif()
