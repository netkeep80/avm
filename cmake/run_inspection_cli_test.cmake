if(NOT DEFINED AVM_INSPECT_EXECUTABLE)
    message(FATAL_ERROR "AVM_INSPECT_EXECUTABLE is required")
endif()

if(NOT DEFINED CASE)
    message(FATAL_ERROR "CASE is required")
endif()

set(script_path "${CMAKE_CURRENT_BINARY_DIR}/inspection-${CASE}.avm")

if(CASE STREQUAL "success")
    set(script "# comment\n\nfind 999999 999998\noutgoing 999999\n")
    set(expected_code 0)
    set(expected_output "find begin=999999 end=999998 id=-\noutgoing endpoint=999999 ids=[]\n")
    set(expected_error "")
elseif(CASE STREQUAL "parser-error")
    set(script "find 999999 999998\nunknown-command\nfind 1 1\n")
    set(expected_code 1)
    set(expected_output "find begin=999999 end=999998 id=-\n")
    set(expected_error "line 2: unknown inspection command: unknown-command\n")
else()
    message(FATAL_ERROR "Unknown avm-inspect test case: ${CASE}")
endif()

file(WRITE "${script_path}" "${script}")
execute_process(
    COMMAND "${AVM_INSPECT_EXECUTABLE}" "${script_path}"
    RESULT_VARIABLE actual_code
    OUTPUT_VARIABLE actual_output
    ERROR_VARIABLE actual_error)

string(REPLACE "\r\n" "\n" actual_output "${actual_output}")
string(REPLACE "\r\n" "\n" actual_error "${actual_error}")

if(NOT actual_code EQUAL expected_code)
    message(FATAL_ERROR "avm-inspect exit code: expected ${expected_code}, got ${actual_code}")
endif()
if(NOT actual_output STREQUAL expected_output)
    message(FATAL_ERROR "avm-inspect stdout mismatch:\n${actual_output}")
endif()
if(NOT actual_error STREQUAL expected_error)
    message(FATAL_ERROR "avm-inspect stderr mismatch:\n${actual_error}")
endif()
