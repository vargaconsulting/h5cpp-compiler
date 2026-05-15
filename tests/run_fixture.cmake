# Drive a single fixture run.
#
# Inputs (passed via -D on the cmake -P invocation):
#   H5CPP_BIN  : absolute path to the h5cpp executable
#   FIXTURE    : absolute path to the fixture source file (.cpp)
#   STUB_DIR   : include directory containing tests/stub/h5cpp_stub.hpp
#   GOLDEN     : absolute path to the golden expected output (may not exist)
#   OUTPUT_DIR : directory for the observed output
#
# Behaviour:
#   - Runs h5cpp on the fixture, capturing the generated header.
#   - Normalises the random include guard so the output is byte-stable.
#   - If GOLDEN exists, diffs against it; mismatch fails the test.
#   - If GOLDEN does not exist, prints a hint with the observed path and passes.
#     This lets the first CI run establish a baseline that can be reviewed and
#     committed as golden in a follow-up.

cmake_minimum_required(VERSION 3.14)

get_filename_component(fixture_name "${FIXTURE}" NAME_WE)
set(observed "${OUTPUT_DIR}/${fixture_name}.observed")

# VS 2026 MSVC STL rejects older clang-cl versions via STL1000.
set(_extra_flags "")
if(CMAKE_HOST_WIN32)
  list(APPEND _extra_flags "-D_ALLOW_COMPILER_AND_STL_VERSION_MISMATCH")
endif()

execute_process(
  COMMAND
    "${H5CPP_BIN}"
    "${FIXTURE}"
    --
    -std=c++17
    "-I${STUB_DIR}"
    "-D${observed}"
    ${_extra_flags}
  RESULT_VARIABLE rc
  OUTPUT_VARIABLE tool_stdout
  ERROR_VARIABLE  tool_stderr
)

if(NOT rc EQUAL 0)
  message(FATAL_ERROR
    "h5cpp exit ${rc} on ${fixture_name}\n"
    "--- stdout ---\n${tool_stdout}\n"
    "--- stderr ---\n${tool_stderr}")
endif()

if(NOT EXISTS "${observed}")
  message(FATAL_ERROR "h5cpp produced no output for ${fixture_name} at ${observed}")
endif()

file(READ "${observed}" content)
string(REGEX REPLACE
  "H5CPP_GUARD_[A-Za-z][A-Za-z][A-Za-z][A-Za-z][A-Za-z]"
  "H5CPP_GUARD_XXXXX"
  content "${content}")
file(WRITE "${observed}" "${content}")

if(EXISTS "${GOLDEN}")
  file(READ "${GOLDEN}" expected)
  if(NOT "${content}" STREQUAL "${expected}")
    message(FATAL_ERROR
      "Golden mismatch for ${fixture_name}\n"
      "  observed: ${observed}\n"
      "  golden:   ${GOLDEN}\n"
      "Refresh by copying observed over golden after manual review.")
  endif()
else()
  message(STATUS "No golden for ${fixture_name}; baseline observation written to ${observed}")
endif()
