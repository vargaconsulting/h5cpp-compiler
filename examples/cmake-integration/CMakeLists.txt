cmake_minimum_required(VERSION 3.14)
project(h5cpp-cmake-example LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Point CMake at the directory containing FindH5CPPCompiler.cmake
# In a real project this would come from CMAKE_PREFIX_PATH or the system.
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../cmake")

find_package(H5CPPCompiler REQUIRED)

# A translation unit that references POD structs via h5:: operators
set(H5CPP_INPUT  "${CMAKE_CURRENT_SOURCE_DIR}/experiment.cpp")
set(H5CPP_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/experiment_h5.hpp")

# Register the code-generation step.
h5cpp_compiler_generate(
    INPUT  ${H5CPP_INPUT}
    OUTPUT ${H5CPP_OUTPUT}
    STD    c++17
)

# The generated header is a build product; mark it as a source so CMake
# knows the target depends on the custom command above.
add_executable(demo experiment.cpp ${H5CPP_OUTPUT})
target_include_directories(demo PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

# If you have h5cpp installed, link it here:
# target_link_libraries(demo PRIVATE h5cpp::all)
