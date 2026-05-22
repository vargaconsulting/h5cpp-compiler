# H5CPPCompilerFunctions.cmake
# Helper functions for invoking h5cpp-compiler from CMake builds.
#
# h5cpp_compiler_generate(
#     INPUT  <source.cpp>
#     OUTPUT <generated.hpp>
#     [STD <standard>]           default: c++17
#     [FORMAT <fmt>]             output format: hdf5 (default) | protocol-buffers
#     [STUB_DIR <dir>]           extra include dir (e.g. for h5cpp_stub.hpp)
#     [EXTRA_FLAGS <flag> ...]   additional flags passed after --
# )
#
# The function registers an add_custom_command so the output is regenerated
# whenever the input file (or the compiler itself) changes.

function(h5cpp_compiler_generate)
    set(_options)
    set(_oneValueArgs INPUT OUTPUT STD STUB_DIR FORMAT)
    set(_multiValueArgs EXTRA_FLAGS)
    cmake_parse_arguments(ARG "${_options}" "${_oneValueArgs}" "${_multiValueArgs}" ${ARGN})

    if(NOT ARG_INPUT)
        message(FATAL_ERROR "h5cpp_compiler_generate: INPUT required")
    endif()
    if(NOT ARG_OUTPUT)
        message(FATAL_ERROR "h5cpp_compiler_generate: OUTPUT required")
    endif()
    if(NOT ARG_STD)
        set(ARG_STD c++17)
    endif()
    if(NOT ARG_FORMAT)
        set(ARG_FORMAT hdf5)
    endif()
    if(NOT ARG_FORMAT MATCHES "^(hdf5|protocol-buffers)$")
        message(FATAL_ERROR "h5cpp_compiler_generate: FORMAT must be hdf5 or protocol-buffers (got '${ARG_FORMAT}')")
    endif()

    get_filename_component(_input_abs  "${ARG_INPUT}"  ABSOLUTE)
    get_filename_component(_output_abs "${ARG_OUTPUT}" ABSOLUTE)

    set(_cmd "${H5CPP_COMPILER}")
    list(APPEND _cmd "--${ARG_FORMAT}")
    list(APPEND _cmd -o "${_output_abs}")
    list(APPEND _cmd "${_input_abs}" -- -std=${ARG_STD})
    if(ARG_STUB_DIR)
        list(APPEND _cmd -I"${ARG_STUB_DIR}")
    endif()
    if(ARG_EXTRA_FLAGS)
        list(APPEND _cmd ${ARG_EXTRA_FLAGS})
    endif()

    add_custom_command(
        OUTPUT "${_output_abs}"
        COMMAND ${_cmd}
        DEPENDS "${_input_abs}"
        COMMENT "h5cpp-compiler: generating ${_output_abs}"
        VERBATIM
    )
endfunction()

# h5cpp_compiler_check(
#     INPUT  <source.cpp>
#     OUTPUT <generated.hpp>
#     [STD <standard>]
#     [FORMAT <fmt>]             output format: hdf5 (default) | protocol-buffers
#     [STUB_DIR <dir>]
#     [EXTRA_FLAGS <flag> ...]
# )
#
# Verifies that <generated.hpp> is up to date with <source.cpp>.
# Fails the build if the generated file would change.
# Intended for CI gating.

function(h5cpp_compiler_check)
    set(_options)
    set(_oneValueArgs INPUT OUTPUT STD STUB_DIR FORMAT)
    set(_multiValueArgs EXTRA_FLAGS)
    cmake_parse_arguments(ARG "${_options}" "${_oneValueArgs}" "${_multiValueArgs}" ${ARGN})

    if(NOT ARG_INPUT)
        message(FATAL_ERROR "h5cpp_compiler_check: INPUT required")
    endif()
    if(NOT ARG_OUTPUT)
        message(FATAL_ERROR "h5cpp_compiler_check: OUTPUT required")
    endif()
    if(NOT ARG_STD)
        set(ARG_STD c++17)
    endif()
    if(NOT ARG_FORMAT)
        set(ARG_FORMAT hdf5)
    endif()
    if(NOT ARG_FORMAT MATCHES "^(hdf5|protocol-buffers)$")
        message(FATAL_ERROR "h5cpp_compiler_check: FORMAT must be hdf5 or protocol-buffers (got '${ARG_FORMAT}')")
    endif()

    get_filename_component(_input_abs  "${ARG_INPUT}"  ABSOLUTE)
    get_filename_component(_output_abs "${ARG_OUTPUT}" ABSOLUTE)

    set(_cmd "${H5CPP_COMPILER}")
    list(APPEND _cmd "--${ARG_FORMAT}")
    list(APPEND _cmd --check)
    list(APPEND _cmd -o "${_output_abs}")
    list(APPEND _cmd "${_input_abs}" -- -std=${ARG_STD})
    if(ARG_STUB_DIR)
        list(APPEND _cmd -I"${ARG_STUB_DIR}")
    endif()
    if(ARG_EXTRA_FLAGS)
        list(APPEND _cmd ${ARG_EXTRA_FLAGS})
    endif()

    add_custom_target(h5cpp-check-${ARG_OUTPUT}
        COMMAND ${_cmd}
        DEPENDS "${_input_abs}"
        COMMENT "h5cpp-compiler: checking ${_output_abs} is up to date"
        VERBATIM
    )
endfunction()
