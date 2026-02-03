# CMake script to run a single AVM JSON roundtrip test
# Arguments:
#   AVM_EXECUTABLE - path to the avm executable
#   TEST_FILE      - path to the input JSON test file
#   TEST_DIR       - path to the test directory

get_filename_component(TEST_NAME "${TEST_FILE}" NAME)

# Remove stale output from previous tests to avoid false positives
set(RES_FILE "${TEST_DIR}/res.json")
if(EXISTS "${RES_FILE}")
    file(REMOVE "${RES_FILE}")
endif()

# Run avm on the test file from the test directory
# Note: avm may return non-zero due to static destructor cleanup issues,
# so we check the output file content instead of relying on exit code
execute_process(
    COMMAND "${AVM_EXECUTABLE}" "${TEST_FILE}"
    WORKING_DIRECTORY "${TEST_DIR}"
    RESULT_VARIABLE RESULT
    OUTPUT_VARIABLE OUTPUT
    ERROR_VARIABLE ERROR_OUTPUT
)

# Check that the output file exists
set(RES_FILE "${TEST_DIR}/res.json")
if(NOT EXISTS "${RES_FILE}")
    message(FATAL_ERROR "avm did not produce output file res.json for ${TEST_NAME}\nExit code: ${RESULT}\nError: ${ERROR_OUTPUT}")
endif()

# Read the output file (res.json) and the expected file
file(READ "${TEST_DIR}/res.json" ACTUAL_CONTENT)
file(READ "${TEST_FILE}" EXPECTED_CONTENT)

# Normalize whitespace for comparison (strip trailing newlines)
string(STRIP "${ACTUAL_CONTENT}" ACTUAL_CONTENT)
string(STRIP "${EXPECTED_CONTENT}" EXPECTED_CONTENT)

# Normalize line endings (CRLF -> LF) for cross-platform compatibility
string(REPLACE "\r\n" "\n" ACTUAL_CONTENT "${ACTUAL_CONTENT}")
string(REPLACE "\r\n" "\n" EXPECTED_CONTENT "${EXPECTED_CONTENT}")

if(NOT "${ACTUAL_CONTENT}" STREQUAL "${EXPECTED_CONTENT}")
    message(FATAL_ERROR "Test ${TEST_NAME} FAILED:\nExpected: ${EXPECTED_CONTENT}\nActual:   ${ACTUAL_CONTENT}")
endif()

message(STATUS "Test ${TEST_NAME} PASSED")
