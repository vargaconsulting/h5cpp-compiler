# Drive a single fixture run.
#
# Inputs (passed via -D on the cmake -P invocation):
#   H5CPP_BIN      : absolute path to the h5cpp executable
#   FIXTURE        : absolute path to the fixture source file (.cpp)
#   STUB_DIR       : include directory containing tests/stub/h5cpp_stub.hpp
#   GOLDEN         : absolute path to the golden expected output (may not exist)
#   BACKEND_FORMAT : backend format string (optional, defaults to hdf5)
#   OUTPUT_DIR     : directory for the observed output
#   PROTO_GOLDEN   : optional — when set (protobuf backend only), also runs
#                    h5cpp with --proto-out and diffs the emitted .proto schema.
#
# Behaviour:
#   - Runs h5cpp on the fixture with --format <BACKEND_FORMAT> -o <observed>.
#   - Normalises the random include guard so the output is byte-stable.
#   - If GOLDEN exists, diffs against it; mismatch fails the test.
#   - If GOLDEN does not exist, prints a hint with the observed path and passes.
#   - If PROTO_GOLDEN is set, repeats the diff for the .proto output.

cmake_minimum_required(VERSION 3.14)

get_filename_component(fixture_name "${FIXTURE}" NAME_WE)

if(NOT DEFINED BACKEND_FORMAT OR BACKEND_FORMAT STREQUAL "")
  set(BACKEND_FORMAT "hdf5")
endif()

if(BACKEND_FORMAT STREQUAL "hdf5")
  set(observed "${OUTPUT_DIR}/${fixture_name}.observed")
else()
  set(observed "${OUTPUT_DIR}/${fixture_name}.${BACKEND_FORMAT}.observed")
endif()

set(proto_observed "")
set(proto_flags "")
if(DEFINED PROTO_GOLDEN AND NOT PROTO_GOLDEN STREQUAL "")
  set(proto_observed "${OUTPUT_DIR}/${fixture_name}.proto.observed")
  list(APPEND proto_flags "--proto-out" "${proto_observed}")
endif()

execute_process(
  COMMAND
    "${H5CPP_BIN}"
    "--${BACKEND_FORMAT}"
    -o "${observed}"
    ${proto_flags}
    "${FIXTURE}"
    --
    -std=c++17
    "-I${STUB_DIR}"
  RESULT_VARIABLE rc
  OUTPUT_VARIABLE tool_stdout
  ERROR_VARIABLE  tool_stderr
)

if(NOT rc EQUAL 0)
  message(FATAL_ERROR
    "h5cpp exit ${rc} on ${fixture_name} (format=${BACKEND_FORMAT})\n"
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
      "Golden mismatch for ${fixture_name} (format=${BACKEND_FORMAT})\n"
      "  observed: ${observed}\n"
      "  golden:   ${GOLDEN}\n"
      "Refresh by copying observed over golden after manual review.")
  endif()
else()
  message(STATUS "No golden for ${fixture_name}; baseline observation written to ${observed}")
endif()

# Optional .proto golden check.
if(DEFINED PROTO_GOLDEN AND NOT PROTO_GOLDEN STREQUAL "")
  if(NOT EXISTS "${proto_observed}")
    message(FATAL_ERROR "h5cpp produced no .proto output at ${proto_observed}")
  endif()
  if(EXISTS "${PROTO_GOLDEN}")
    file(READ "${proto_observed}" proto_content)
    file(READ "${PROTO_GOLDEN}"   proto_expected)
    if(NOT "${proto_content}" STREQUAL "${proto_expected}")
      message(FATAL_ERROR
        "Proto golden mismatch for ${fixture_name}\n"
        "  observed: ${proto_observed}\n"
        "  golden:   ${PROTO_GOLDEN}\n"
        "Refresh by copying observed over golden after manual review.")
    endif()
  else()
    message(STATUS "No proto golden for ${fixture_name}; baseline observation at ${proto_observed}")
  endif()
endif()
