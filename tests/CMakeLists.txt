set(STUB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/stub)
set(GOLDEN_DIR ${CMAKE_CURRENT_SOURCE_DIR}/golden)
set(RUN_FIXTURE ${CMAKE_CURRENT_SOURCE_DIR}/run_fixture.cmake)

set(FIXTURES
  primitives
  typedef
  nested-ns
  embedded-pod
  array-of-arrays
  ignored-non-pod
  unreferenced
  topological
  enum-member
  multi-array
  inheritance
  duplicate-struct
  class-type
)

function(add_h5cpp_fixture_test name)
  set(extra_args "")
  if(ARGC GREATER 1)
    set(extra_args "-DEXTRA_ARGS=${ARGV1}")
  endif()
  add_test(
    NAME h5cpp.${name}
    COMMAND
      ${CMAKE_COMMAND}
        "-DH5CPP_BIN=$<TARGET_FILE:h5cpp>"
        "-DFIXTURE=${CMAKE_CURRENT_SOURCE_DIR}/fixtures/${name}.cpp"
        "-DSTUB_DIR=${STUB_DIR}"
        "-DGOLDEN=${GOLDEN_DIR}/${name}.expected"
        "-DOUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}"
        ${extra_args}
        -P "${RUN_FIXTURE}"
  )
endfunction()

foreach(fixture IN LISTS FIXTURES)
  add_h5cpp_fixture_test(${fixture})
endforeach()

# Legacy-mode fixture: exercises producer_h5_legacy.hpp
add_h5cpp_fixture_test(legacy "--legacy-register-struct")

# --check mode tests
set(CHECK_FIXTURE "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/primitives.cpp")

add_test(
  NAME h5cpp.check-pass
  COMMAND ${CMAKE_COMMAND}
    -DH5CPP_BIN=$<TARGET_FILE:h5cpp>
    -DFIXTURE=${CHECK_FIXTURE}
    -DSTUB_DIR=${STUB_DIR}
    -P "${CMAKE_CURRENT_SOURCE_DIR}/run_check_pass.cmake"
)

add_test(
  NAME h5cpp.check-fail
  COMMAND ${CMAKE_COMMAND}
    -DH5CPP_BIN=$<TARGET_FILE:h5cpp>
    -DFIXTURE=${CHECK_FIXTURE}
    -DSTUB_DIR=${STUB_DIR}
    -P "${CMAKE_CURRENT_SOURCE_DIR}/run_check_fail.cmake"
)
