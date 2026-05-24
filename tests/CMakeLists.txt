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
  h5_name
  h5_ignore
  h5_scatter
  h5_scatter_attrs
  h5_chunk
  h5_compress
  h5_chunk_compress
  h5_metadata
  h5_name_all
  h5_serialize_full
  h5_on_missing
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

function(add_h5cpp_backend_fixture_test name backend)
  set(golden "${GOLDEN_DIR}/${name}.${backend}.expected")
  add_test(
    NAME h5cpp.${name}
    COMMAND
      ${CMAKE_COMMAND}
        "-DH5CPP_BIN=$<TARGET_FILE:h5cpp>"
        "-DFIXTURE=${CMAKE_CURRENT_SOURCE_DIR}/fixtures/${name}.cpp"
        "-DSTUB_DIR=${STUB_DIR}"
        "-DGOLDEN=${golden}"
        "-DBACKEND_FORMAT=${backend}"
        "-DOUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}"
        -P "${RUN_FIXTURE}"
  )
endfunction()

foreach(fixture IN LISTS FIXTURES)
  add_h5cpp_fixture_test(${fixture})
endforeach()

set(JSON_FIXTURES
  json_primitives
  json_arrays
  json_nested
  json_enums
  json_strings
)

foreach(fixture IN LISTS JSON_FIXTURES)
  add_h5cpp_backend_fixture_test(${fixture} json)
endforeach()
