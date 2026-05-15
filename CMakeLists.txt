#  _____________________________________________________________________________
#  Copyright (c) 2018-2025 Steven Varga, Toronto,ON Canada
#  Author: Varga, Steven <steven@vargaconsulting.ca>
#  _____________________________________________________________________________

cmake_minimum_required(VERSION 3.14)
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

if(NOT MSVC)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti")
endif()

# Explicit executable target
add_executable(h5cpp src/h5cpp.cpp)

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

option(H5CPP_BUILD_TESTS "Build h5cpp-compiler fixture tests" ON)
if(H5CPP_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
