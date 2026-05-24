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
)

function(add_h5cpp_fixture_test name)
  add_test(
    NAME h5cpp.${name}
    COMMAND
      ${CMAKE_COMMAND}
        "-DH5CPP_BIN=$<TARGET_FILE:h5cpp>"
        "-DFIXTURE=${CMAKE_CURRENT_SOURCE_DIR}/fixtures/${name}.cpp"
        "-DSTUB_DIR=${STUB_DIR}"
        "-DGOLDEN=${GOLDEN_DIR}/${name}.expected"
        "-DOUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}"
        -P "${RUN_FIXTURE}"
  )
endfunction()

function(add_h5cpp_backend_test name backend)
  add_test(
    NAME h5cpp.${name}.${backend}
    COMMAND
      ${CMAKE_COMMAND}
        "-DH5CPP_BIN=$<TARGET_FILE:h5cpp>"
        "-DFIXTURE=${CMAKE_CURRENT_SOURCE_DIR}/fixtures/${name}.cpp"
        "-DSTUB_DIR=${STUB_DIR}"
        "-DGOLDEN=${GOLDEN_DIR}/${name}.${backend}.expected"
        "-DOUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}"
        "-DBACKEND_FORMAT=${backend}"
        -P "${RUN_FIXTURE}"
  )
endfunction()

foreach(fixture IN LISTS FIXTURES)
  add_h5cpp_fixture_test(${fixture})
endforeach()

foreach(fixture IN LISTS FIXTURES)
  add_h5cpp_backend_test(${fixture} sql-postgres)
  add_h5cpp_backend_test(${fixture} sql-mysql)
  add_h5cpp_backend_test(${fixture} sql-lite3)
endforeach()
