#  _____________________________________________________________________________
#  Copyright (c) 2018-2025 Steven Varga, Toronto,ON Canada
#  Author: Varga, Steven <steven@vargaconsulting.ca>
#  _____________________________________________________________________________

cmake_minimum_required(VERSION 3.14)
if(POLICY CMP0091)
  cmake_policy(SET CMP0091 NEW)
endif()
project(h5cpp-compiler-dev VERSION 1.10.4.6 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT MSVC)
  set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -s")
endif()

if(WIN32 AND NOT LLVM_DIR)
  list(APPEND CMAKE_PREFIX_PATH "C:/Program Files/LLVM")
endif()

find_package(LLVM REQUIRED CONFIG)
find_package(Clang REQUIRED CONFIG)

message(STATUS "Using LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "LLVM_DIR: ${LLVM_DIR}")
message(STATUS "CLANG_DIR: ${Clang_DIR}")

if(APPLE AND LLVM_TOOLS_BINARY_DIR AND LLVM_VERSION_MAJOR)
  # LibTooling resolves its resource dir as <binary-dir>/../lib/clang/<major>.
  # We place the binary in build/bin/ so that path resolves to build/lib/clang/<major>,
  # then symlink that to Homebrew LLVM's actual resource directory.
  get_filename_component(_llvm_prefix "${LLVM_TOOLS_BINARY_DIR}" DIRECTORY)
  set(_rdir_src "${_llvm_prefix}/lib/clang/${LLVM_VERSION_MAJOR}")
  set(_rdir_dst "${CMAKE_BINARY_DIR}/lib/clang/${LLVM_VERSION_MAJOR}")
  if(EXISTS "${_rdir_src}" AND NOT EXISTS "${_rdir_dst}")
    file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/lib/clang")
    file(CREATE_LINK "${_rdir_src}" "${_rdir_dst}" SYMBOLIC)
    message(STATUS "Linked resource dir: ${_rdir_dst} -> ${_rdir_src}")
  endif()
endif()

if(NOT MSVC)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti")
endif()

# Explicit executable target
add_executable(h5cpp src/h5cpp.cpp)

if(APPLE)
  # Keep the binary one level below the build root so LibTooling's resource dir
  # search (binary-dir/../lib/clang/<major>) resolves to build/lib/clang/<major>.
  set_target_properties(h5cpp PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
  )
endif()

# Modern include + defs
target_include_directories(h5cpp PRIVATE
  ${LLVM_INCLUDE_DIRS}
  ${CLANG_INCLUDE_DIRS}
)

target_compile_definitions(h5cpp PRIVATE
  ${LLVM_DEFINITIONS}
)

# Link against modern LLVM/Clang targets
target_link_libraries(h5cpp
  PRIVATE
    clangTooling
)

# Install directives
include(GNUInstallDirs)
install(TARGETS h5cpp DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES h5cpp.1 DESTINATION ${CMAKE_INSTALL_MANDIR}/man1)

# CMake helper modules for consumers
install(
    DIRECTORY cmake/
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/cmake/h5cpp-compiler
    FILES_MATCHING PATTERN "*.cmake"
)

option(H5CPP_BUILD_TESTS "Build h5cpp-compiler fixture tests" ON)
if(H5CPP_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
