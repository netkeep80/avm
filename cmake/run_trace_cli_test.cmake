# AVM trace CLI conformance harness.
# Required:
#   AVM_EXECUTABLE
#   TEST_DIR
#   CASE = usage | legacy | success | function | truncate | failure | nonexpression | badlimit
# TEST_FILE is required for all cases except usage.

if(NOT DEFINED AVM_EXECUTABLE OR NOT DEFINED TEST_DIR OR NOT DEFINED CASE)
    message(FATAL_ERROR "AVM_EXECUTABLE, TEST_DIR and CASE are required")
endif()

set(RES_FILE "${TEST_DIR}/res.json")
set(DUMP_FILE "${TEST_DIR}/rvm.dump.json")
file(REMOVE "${RES_FILE}" "${DUMP_FILE}")

if(CASE STREQUAL "usage")
    execute_process(
        COMMAND "${AVM_EXECUTABLE}"
        WORKING_DIRECTORY "${TEST_DIR}"
        RESULT_VARIABLE RESULT
        OUTPUT_VARIABLE OUTPUT
        ERROR_VARIABLE ERROR_OUTPUT)
elseif(CASE STREQUAL "legacy")
    execute_process(
        COMMAND "${AVM_EXECUTABLE}" "${TEST_FILE}"
        WORKING_DIRECTORY "${TEST_DIR}"
        RESULT_VARIABLE RESULT
        OUTPUT_VARIABLE OUTPUT
        ERROR_VARIABLE ERROR_OUTPUT)
elseif(CASE STREQUAL "truncate")
    execute_process(
        COMMAND "${AVM_EXECUTABLE}" --trace-limit 1 "${TEST_FILE}"
        WORKING_DIRECTORY "${TEST_DIR}"
        RESULT_VARIABLE RESULT
        OUTPUT_VARIABLE OUTPUT
        ERROR_VARIABLE ERROR_OUTPUT)
elseif(CASE STREQUAL "badlimit")
    execute_process(
        COMMAND "${AVM_EXECUTABLE}" --trace-limit nope "${TEST_FILE}"
        WORKING_DIRECTORY "${TEST_DIR}"
        RESULT_VARIABLE RESULT
        OUTPUT_VARIABLE OUTPUT
        ERROR_VARIABLE ERROR_OUTPUT)
else()
    execute_process(
        COMMAND "${AVM_EXECUTABLE}" --trace "${TEST_FILE}"
        WORKING_DIRECTORY "${TEST_DIR}"
        RESULT_VARIABLE RESULT
        OUTPUT_VARIABLE OUTPUT
        ERROR_VARIABLE ERROR_OUTPUT)
endif()

if(CASE STREQUAL "usage")
    if(NOT RESULT EQUAL 0)
        message(FATAL_ERROR "usage command failed: ${RESULT}\n${ERROR_OUTPUT}")
    endif()
    string(FIND "${OUTPUT}" "Associative Virtual Machine [Version 1.3.0]" VERSION_POS)
    if(VERSION_POS EQUAL -1)
        message(FATAL_ERROR "usage banner does not expose AVM 1.3.0:\n${OUTPUT}")
    endif()
    string(FIND "${OUTPUT}" "avm --trace <entry_point>" TRACE_USAGE_POS)
    if(TRACE_USAGE_POS EQUAL -1)
        message(FATAL_ERROR "usage banner does not document trace mode:\n${OUTPUT}")
    endif()
    return()
endif()

if(CASE STREQUAL "failure" OR CASE STREQUAL "nonexpression" OR CASE STREQUAL "badlimit")
    if(RESULT EQUAL 0)
        message(FATAL_ERROR "${CASE} unexpectedly succeeded\nstdout:\n${OUTPUT}\nstderr:\n${ERROR_OUTPUT}")
    endif()
    if(EXISTS "${RES_FILE}")
        message(FATAL_ERROR "${CASE} unexpectedly produced res.json")
    endif()
else()
    if(NOT RESULT EQUAL 0)
        message(FATAL_ERROR "${CASE} failed: ${RESULT}\nstdout:\n${OUTPUT}\nstderr:\n${ERROR_OUTPUT}")
    endif()
    if(NOT EXISTS "${RES_FILE}")
        message(FATAL_ERROR "${CASE} did not produce res.json")
    endif()
    file(READ "${RES_FILE}" ACTUAL_RESULT)
    string(STRIP "${ACTUAL_RESULT}" ACTUAL_RESULT)
    if(NOT "${ACTUAL_RESULT}" STREQUAL "true")
        message(FATAL_ERROR "${CASE} expected res.json=true, got: ${ACTUAL_RESULT}")
    endif()
endif()

if(CASE STREQUAL "legacy")
    string(FIND "${OUTPUT}" "trace events=" TRACE_POS)
    if(NOT TRACE_POS EQUAL -1)
        message(FATAL_ERROR "legacy mode unexpectedly printed trace output:\n${OUTPUT}")
    endif()
elseif(CASE STREQUAL "success")
    string(FIND "${OUTPUT}" "enter entity=" ENTER_POS)
    string(FIND "${OUTPUT}" "return entity=" RETURN_POS)
    string(FIND "${OUTPUT}" "trace events=8 complete=true truncated=false" SUMMARY_POS)
    if(ENTER_POS EQUAL -1 OR RETURN_POS EQUAL -1 OR SUMMARY_POS EQUAL -1)
        message(FATAL_ERROR "successful trace output is incomplete:\n${OUTPUT}")
    endif()
elseif(CASE STREQUAL "function")
    string(REGEX MATCH "frame=[0-9]+" FRAME_MATCH "${OUTPUT}")
    string(FIND "${OUTPUT}" "trace events=" SUMMARY_POS)
    string(FIND "${OUTPUT}" "complete=true truncated=false" COMPLETE_POS)
    if("${FRAME_MATCH}" STREQUAL "" OR SUMMARY_POS EQUAL -1 OR COMPLETE_POS EQUAL -1)
        message(FATAL_ERROR "function trace did not expose a frame-bearing complete trace:\n${OUTPUT}")
    endif()
elseif(CASE STREQUAL "truncate")
    string(FIND "${OUTPUT}" "trace events=1 complete=false truncated=true" SUMMARY_POS)
    if(SUMMARY_POS EQUAL -1)
        message(FATAL_ERROR "trace truncation was not reported explicitly:\n${OUTPUT}")
    endif()
elseif(CASE STREQUAL "failure")
    string(FIND "${OUTPUT}" "fail entity=" FAIL_POS)
    string(FIND "${OUTPUT}" "phase=handler" PHASE_POS)
    string(FIND "${OUTPUT}" "complete=true truncated=false" COMPLETE_POS)
    string(FIND "${ERROR_OUTPUT}" "undefined function handle" ERROR_POS)
    if(FAIL_POS EQUAL -1 OR PHASE_POS EQUAL -1 OR COMPLETE_POS EQUAL -1 OR ERROR_POS EQUAL -1)
        message(FATAL_ERROR "failure trace/diagnostic mismatch:\nstdout:\n${OUTPUT}\nstderr:\n${ERROR_OUTPUT}")
    endif()
elseif(CASE STREQUAL "nonexpression")
    string(FIND "${OUTPUT}" "trace events=" TRACE_POS)
    string(FIND "${ERROR_OUTPUT}" "--trace requires an executable JSON compatibility expression" ERROR_POS)
    if(NOT TRACE_POS EQUAL -1 OR ERROR_POS EQUAL -1)
        message(FATAL_ERROR "non-expression trace rejection mismatch:\nstdout:\n${OUTPUT}\nstderr:\n${ERROR_OUTPUT}")
    endif()
elseif(CASE STREQUAL "badlimit")
    string(FIND "${OUTPUT}" "trace events=" TRACE_POS)
    string(FIND "${ERROR_OUTPUT}" "trace limit must be a non-negative integer" ERROR_POS)
    if(NOT TRACE_POS EQUAL -1 OR ERROR_POS EQUAL -1)
        message(FATAL_ERROR "invalid trace-limit handling mismatch:\nstdout:\n${OUTPUT}\nstderr:\n${ERROR_OUTPUT}")
    endif()
else()
    message(FATAL_ERROR "unknown trace CLI test CASE: ${CASE}")
endif()

file(REMOVE "${RES_FILE}" "${DUMP_FILE}")
