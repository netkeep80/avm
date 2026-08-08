cmake_minimum_required(VERSION 3.20 FATAL_ERROR)
message("Configuring: ${CMAKE_CURRENT_SOURCE_DIR}")

set(CMAKE_PROJECT_VERSION 1.3.0)

project(avm
    VERSION ${CMAKE_PROJECT_VERSION}
    DESCRIPTION "Associative virtual machine"
    LANGUAGES CXX)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

enable_testing()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(avm_core INTERFACE)
add_library(avm::core ALIAS avm_core)
set_target_properties(avm_core PROPERTIES EXPORT_NAME core)
target_compile_features(avm_core INTERFACE cxx_std_20)
target_include_directories(avm_core INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

set(AVM_CORE_PUBLIC_HEADERS
    include/avm/avm.h
    include/avm/bootstrap_runtime.h
    include/avm/execution_observer.h
    include/avm/execution_trace.h
    include/avm/executor.h
    include/avm/link_store.h
    include/avm/persistent_link_store.h
    include/avm/program_model.h
    include/avm/projection.h
    include/avm/raw_carrier.h
    include/avm/relations_model.h
    include/avm/relations_query.h
    include/avm/version.h)

install(TARGETS avm_core EXPORT avmTargets)
install(FILES ${AVM_CORE_PUBLIC_HEADERS} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/avm)
install(EXPORT avmTargets
    FILE avmTargets.cmake
    NAMESPACE avm::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/avm)

configure_package_config_file(
    cmake/avmConfig.cmake.in
    "${CMAKE_CURRENT_BINARY_DIR}/avmConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/avm)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/avmConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion)
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/avmConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/avmConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/avm)

option(AVM_BUILD_CLI "Build the AVM JSON CLI" ON)
option(AVM_BUILD_CORE_TESTS "Build AVM core tests" ON)
option(AVM_BUILD_JSON_COMPAT_TESTS "Build JSON projection/session tests" ON)
option(AVM_BUILD_ANUM_ADAPTER_TESTS "Build canonical Anum denotation adapter tests" ON)
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

function(avm_add_json_includes target)
    target_link_libraries(${target} PRIVATE avm_core)
    target_include_directories(${target} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/3p")
endfunction()

if(AVM_BUILD_CLI)
    add_executable(avm "${CMAKE_CURRENT_SOURCE_DIR}/src/cli.cpp")
    avm_add_json_includes(avm)
    avm_apply_quality(avm)

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

    foreach(TEST_FILE ${TEST_FILES})
        add_test(
            NAME "json_roundtrip_${TEST_FILE}"
            COMMAND ${CMAKE_COMMAND}
                -DAVM_EXECUTABLE=$<TARGET_FILE:avm>
                -DTEST_FILE=${TEST_DIR}/${TEST_FILE}
                -DTEST_DIR=${TEST_DIR}
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/run_test.cmake")
    endforeach()
endif()

if(AVM_BUILD_CORE_TESTS)
    function(avm_add_core_test target source test_name)
        add_executable(${target} "${CMAKE_CURRENT_SOURCE_DIR}/${source}")
        target_link_libraries(${target} PRIVATE avm_core)
        avm_apply_quality(${target})
        avm_enable_test_assertions(${target})
        add_test(NAME ${test_name} COMMAND ${target})
    endfunction()

    avm_add_core_test(avm_assertions_enabled_test test/assertions_enabled_test.cpp assertions_enabled_tests)
    avm_add_core_test(avm_link_store_test test/link_store_test.cpp link_store_tests)
    avm_add_core_test(avm_relations_model_test test/relations_model_test.cpp relations_model_tests)
    avm_add_core_test(avm_relations_query_test test/relations_query_test.cpp relations_query_tests)
    avm_add_core_test(avm_executor_test test/executor_test.cpp executor_tests)
    avm_add_core_test(avm_execution_observer_test test/execution_observer_test.cpp execution_observer_tests)
    avm_add_core_test(avm_execution_trace_test test/execution_trace_test.cpp execution_trace_tests)
    avm_add_core_test(
        avm_execution_trace_persistence_test
        test/execution_trace_persistence_test.cpp
        execution_trace_persistence_tests)
    avm_add_core_test(avm_program_model_test test/program_model_test.cpp program_model_tests)
    avm_add_core_test(avm_boolean_runtime_test test/boolean_runtime_test.cpp boolean_runtime_tests)
    avm_add_core_test(avm_structural_runtime_test test/structural_runtime_test.cpp structural_runtime_tests)
    avm_add_core_test(avm_structural_library_test test/structural_library_test.cpp structural_library_tests)
    avm_add_core_test(avm_function_runtime_test test/function_runtime_test.cpp function_runtime_tests)
    avm_add_core_test(avm_frame_runtime_test test/frame_runtime_test.cpp frame_runtime_tests)
    avm_add_core_test(avm_deferred_definition_test test/deferred_definition_test.cpp deferred_definition_tests)
    avm_add_core_test(avm_projection_test test/projection_test.cpp projection_tests)
    avm_add_core_test(avm_raw_carrier_test test/raw_carrier_test.cpp raw_carrier_tests)
    avm_add_core_test(
        avm_protocol_adapter_boundary_test
        test/protocol_adapter_boundary_test.cpp
        protocol_adapter_boundary_tests)
    avm_add_core_test(avm_persistent_link_store_test test/persistent_link_store_test.cpp persistent_link_store_tests)

    add_executable(avm_vertical_slice_test "${CMAKE_CURRENT_SOURCE_DIR}/test/vertical_slice_test.cpp")
    avm_add_json_includes(avm_vertical_slice_test)
    avm_apply_quality(avm_vertical_slice_test)
    avm_enable_test_assertions(avm_vertical_slice_test)
    add_test(NAME vertical_slice_tests COMMAND avm_vertical_slice_test)

    add_executable(avm_json_value_codec_test "${CMAKE_CURRENT_SOURCE_DIR}/test/json_value_codec_test.cpp")
    avm_add_json_includes(avm_json_value_codec_test)
    avm_apply_quality(avm_json_value_codec_test)
    avm_enable_test_assertions(avm_json_value_codec_test)
    add_test(NAME json_value_codec_tests COMMAND avm_json_value_codec_test)

    add_custom_target(avm_core_tests DEPENDS
        avm_assertions_enabled_test
        avm_link_store_test
        avm_relations_model_test
        avm_relations_query_test
        avm_executor_test
        avm_execution_observer_test
        avm_execution_trace_test
        avm_execution_trace_persistence_test
        avm_program_model_test
        avm_boolean_runtime_test
        avm_structural_runtime_test
        avm_structural_library_test
        avm_function_runtime_test
        avm_frame_runtime_test
        avm_deferred_definition_test
        avm_projection_test
        avm_raw_carrier_test
        avm_protocol_adapter_boundary_test
        avm_persistent_link_store_test
        avm_vertical_slice_test
        avm_json_value_codec_test)
endif()

if(AVM_BUILD_ANUM_ADAPTER_TESTS)
    add_executable(avm_anum_denotation_bridge_test "${CMAKE_CURRENT_SOURCE_DIR}/test/anum_denotation_bridge_test.cpp")
    target_link_libraries(avm_anum_denotation_bridge_test PRIVATE avm_core)
    target_include_directories(avm_anum_denotation_bridge_test PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/adapters"
        "${CMAKE_CURRENT_SOURCE_DIR}/3p")
    avm_apply_quality(avm_anum_denotation_bridge_test)
    avm_enable_test_assertions(avm_anum_denotation_bridge_test)
    add_test(
        NAME anum_denotation_bridge_tests
        COMMAND avm_anum_denotation_bridge_test
            "${CMAKE_CURRENT_SOURCE_DIR}/test/conformance/anum-denotation-conformance-v0.2.json"
            "${CMAKE_CURRENT_SOURCE_DIR}/test/conformance/anum-recursive-denotation-conformance-v0.2.json"
            "${CMAKE_CURRENT_SOURCE_DIR}/test/conformance/anum-v0.2-provenance.json")
    add_custom_target(avm_anum_adapter_tests DEPENDS avm_anum_denotation_bridge_test)
endif()

if(AVM_BUILD_JSON_COMPAT_TESTS)
    add_executable(avm_json_projection_test "${CMAKE_CURRENT_SOURCE_DIR}/test/json_projection_test.cpp")
    avm_add_json_includes(avm_json_projection_test)
    avm_apply_quality(avm_json_projection_test)
    avm_enable_test_assertions(avm_json_projection_test)
    add_test(NAME json_projection_tests COMMAND avm_json_projection_test)

    add_executable(avm_json_session_test "${CMAKE_CURRENT_SOURCE_DIR}/test/json_session_test.cpp")
    avm_add_json_includes(avm_json_session_test)
    avm_apply_quality(avm_json_session_test)
    avm_enable_test_assertions(avm_json_session_test)
    add_test(NAME json_session_tests COMMAND avm_json_session_test)

    add_custom_target(avm_json_compat_tests DEPENDS avm_json_projection_test avm_json_session_test)
endif()
