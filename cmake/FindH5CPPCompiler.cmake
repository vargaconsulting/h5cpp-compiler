# FindH5CPPCompiler.cmake
# Locate the h5cpp-compiler executable and provide helper functions.
#
# Variables:
#   H5CPP_COMPILER      - path to the h5cpp executable
#   H5CPPCompiler_FOUND - TRUE if the executable was found
#
# Targets:
#   h5cpp::compiler     - imported executable target
#
# Functions (included automatically):
#   h5cpp_compiler_generate() - register a custom command that runs the compiler
#
# Example:
#   find_package(H5CPPCompiler REQUIRED)
#   h5cpp_compiler_generate(
#       INPUT  ${CMAKE_CURRENT_SOURCE_DIR}/particle.cpp
#       OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/particle_h5.hpp
#   )
#   target_sources(my_app PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/particle_h5.hpp)

find_program(H5CPP_COMPILER
    NAMES h5cpp
    DOC "h5cpp-compiler executable"
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(H5CPPCompiler
    REQUIRED_VARS H5CPP_COMPILER
)

mark_as_advanced(H5CPP_COMPILER)

if(H5CPPCompiler_FOUND AND NOT TARGET h5cpp::compiler)
    add_executable(h5cpp::compiler IMPORTED)
    set_property(TARGET h5cpp::compiler PROPERTY
        IMPORTED_LOCATION "${H5CPP_COMPILER}")
endif()

if(H5CPPCompiler_FOUND)
    include("${CMAKE_CURRENT_LIST_DIR}/H5CPPCompilerFunctions.cmake")
endif()
