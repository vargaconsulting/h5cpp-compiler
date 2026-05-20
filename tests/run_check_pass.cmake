cmake_minimum_required(VERSION 3.14)

get_filename_component(fixture_name "${FIXTURE}" NAME_WE)
set(generated "${CMAKE_CURRENT_BINARY_DIR}/${fixture_name}-check-pass.generated")

# Step 1: generate the header normally
execute_process(
  COMMAND "${H5CPP_BIN}" "${FIXTURE}" -- -std=c++17 "-I${STUB_DIR}" "-D${generated}"
  RESULT_VARIABLE rc
  OUTPUT_VARIABLE tool_stdout
  ERROR_VARIABLE  tool_stderr
)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR
    "h5cpp generation failed for ${fixture_name}\n"
    "--- stderr ---\n${tool_stderr}")
endif()

# Step 2: run --check; must exit 0 because the existing file matches
execute_process(
  COMMAND "${H5CPP_BIN}" "--check" "${FIXTURE}" -- -std=c++17 "-I${STUB_DIR}" "-D${generated}"
  RESULT_VARIABLE rc
  OUTPUT_VARIABLE tool_stdout
  ERROR_VARIABLE  tool_stderr
)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR
    "h5cpp --check should have passed but exited ${rc}\n"
    "--- stderr ---\n${tool_stderr}")
endif()
