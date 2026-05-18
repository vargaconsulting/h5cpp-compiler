#  _____________________________________________________________________________
#  Copyright (c) 2018-2025 Steven Varga, Toronto,ON Canada
#  Author: Varga, Steven <steven@vargaconsulting.ca>
#  _____________________________________________________________________________

cmake_minimum_required(VERSION 3.14)
if(POLICY CMP0091)
  cmake_policy(SET CMP0091 NEW)
endif()
project(h5cpp-compiler-dev VERSION 1.12.0 LANGUAGES CXX C)

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

# Linking
option(H5CPP_STATIC_LINK_LLVM "Static-link LLVM and Clang libraries (Linux only)" OFF)

if(H5CPP_STATIC_LINK_LLVM AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
  # Explicit static library lists
  set(_clang_libs
    clangTooling
    clangFormat
    clangToolingInclusions
    clangToolingCore
    clangRewrite
    clangFrontend
    clangDriver
    clangSerialization
    clangSema
    clangParse
    clangAPINotes
    clangEdit
    clangAnalysis
    clangASTMatchers
    clangAST
    clangSupport
    clangLex
    clangBasic
  )

  set(_llvm_components
    LLVMAggressiveInstCombine
    LLVMAnalysis
    LLVMAsmParser
    LLVMBinaryFormat
    LLVMBitReader
    LLVMBitWriter
    LLVMBitstreamReader
    LLVMCodeGen
    LLVMCodeGenTypes
    LLVMCore
    LLVMDebugInfoDWARF
    LLVMDemangle
    LLVMFrontendAtomic
    LLVMFrontendOffloading
    LLVMFrontendOpenMP
    LLVMIRReader
    LLVMInstCombine
    LLVMInstrumentation
    LLVMLinker
    LLVMMC
    LLVMMCParser
    LLVMObject
    LLVMOption
    LLVMPasses
    LLVMProfileData
    LLVMRemarks
    LLVMScalarOpts
    LLVMSupport
    LLVMTarget
    LLVMWindowsDriver
    LLVMTargetParser
    LLVMTextAPI
    LLVMTransformUtils
    LLVMVectorize
    LLVMX86AsmParser
    LLVMX86CodeGen
    LLVMX86Desc
    LLVMX86Disassembler
    LLVMX86Info
    LLVMipo
  )

  set(_lib_ext ".a")
  set(_llvm_lib_dir "${LLVM_LIBRARY_DIRS}")

  set(_clang_static_libs "")
  foreach(_lib IN LISTS _clang_libs)
    set(_lib_path "${_llvm_lib_dir}/lib${_lib}${_lib_ext}")
    if(EXISTS ${_lib_path})
      list(APPEND _clang_static_libs ${_lib_path})
    else()
      message(FATAL_ERROR "Missing clang static library: ${_lib_path}")
    endif()
  endforeach()

  set(_llvm_static_libs "")
  foreach(_comp IN LISTS _llvm_components)
    set(_lib_path "${_llvm_lib_dir}/lib${_comp}${_lib_ext}")
    if(EXISTS ${_lib_path})
      list(APPEND _llvm_static_libs ${_lib_path})
    else()
      message(WARNING "Missing LLVM static library: ${_lib_path}")
    endif()
  endforeach()

  # zstd may only have a versioned .so without dev package
  if(EXISTS "/usr/lib/x86_64-linux-gnu/libzstd.so.1")
    set(ZSTD_LIBRARY "/usr/lib/x86_64-linux-gnu/libzstd.so.1")
  elseif(EXISTS "/lib/x86_64-linux-gnu/libzstd.so.1")
    set(ZSTD_LIBRARY "/lib/x86_64-linux-gnu/libzstd.so.1")
  else()
    set(ZSTD_LIBRARY "zstd")
  endif()

  target_link_libraries(h5cpp PRIVATE
    ${_clang_static_libs}
    -Wl,--start-group
    ${_llvm_static_libs}
    -Wl,--end-group
    -lz ${ZSTD_LIBRARY} -lxml2 -lffi -ldl -lpthread -lm -lrt
  )
else()
  target_link_libraries(h5cpp PRIVATE clangTooling)
endif()

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

# CPack packaging
set(CPACK_PACKAGE_NAME "h5cpp-compiler")
set(CPACK_PACKAGE_VENDOR "VargaConsulting")
set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Source code transformation tool for HDF5 H5CPP library")
set(CPACK_PACKAGE_CONTACT "steven@vargaconsulting.ca")
set(CPACK_PACKAGE_LICENSE "MIT")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/COPYRIGHT.txt")
set(CPACK_MONOLITHIC_INSTALL 1)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  set(CPACK_PACKAGING_INSTALL_PREFIX "/usr")
  set(CPACK_DEBIAN_PACKAGE_SECTION "devel")
  set(CPACK_DEBIAN_PACKAGE_DEPENDS "libzstd1, zlib1g, libxml2, libffi8")
  set(CPACK_RPM_PACKAGE_LICENSE "MIT")
  set(CPACK_RPM_PACKAGE_GROUP "Development/Tools")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
  set(CPACK_GENERATOR "productbuild")
  set(CPACK_PRODUCTBUILD_IDENTIFIER "ca.vargaconsulting.h5cpp-compiler")
  set(CPACK_PACKAGING_INSTALL_PREFIX "/usr/local")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
  set(CPACK_GENERATOR "NSIS")
  set(CPACK_NSIS_PACKAGE_NAME "h5cpp-compiler")
  set(CPACK_NSIS_INSTALL_ROOT "C:\\\\Program Files\\\\h5cpp-compiler")
endif()

include(CPack)

option(H5CPP_BUILD_TESTS "Build h5cpp-compiler fixture tests" ON)
if(H5CPP_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
