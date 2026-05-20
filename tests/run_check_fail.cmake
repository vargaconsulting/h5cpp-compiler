cmake_minimum_required(VERSION 3.14)

get_filename_component(fixture_name "${FIXTURE}" NAME_WE)
set(generated "${CMAKE_CURRENT_BINARY_DIR}/${fixture_name}-check-fail.generated")

# Step 1: seed a stale generated file
file(WRITE "${generated}" "/* stale content – intentionally out of date */\n")

# Step 2: run --check; must exit 1 because the existing file is stale
execute_process(
  COMMAND "${H5CPP_BIN}" "--check" "${FIXTURE}" -- -std=c++17 "-I${STUB_DIR}" "-D${generated}"
  RESULT_VARIABLE rc
  OUTPUT_VARIABLE tool_stdout
  ERROR_VARIABLE  tool_stderr
)
if(rc EQUAL 0)
  message(FATAL_ERROR
    "h5cpp --check should have failed on stale file but passed")
endif()

string(FIND "${tool_stderr}" "out of date" pos)
if(pos EQUAL -1)
  message(FATAL_ERROR
    "Expected 'out of date' in stderr, got:\n${tool_stderr}")
endif()
