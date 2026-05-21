# h5cpp-compiler
[![CI](https://github.com/vargalabs/h5cpp-compiler/actions/workflows/ci.yml/badge.svg)](https://github.com/vargalabs/h5cpp-compiler/actions/workflows/ci.yml)
[![codecov](https://codecov.io/gh/vargalabs/h5cpp-compiler/branch/release/graph/badge.svg)](https://app.codecov.io/gh/vargalabs/h5cpp-compiler/tree/release)
[![MIT License](https://img.shields.io/badge/license-MIT-green.svg)](LICENSE)
[![DOI](https://zenodo.org/badge/DOI/10.5281/zenodo.17069343.svg)](https://doi.org/10.5281/zenodo.17069343)
[![GitHub release](https://img.shields.io/github/v/release/vargalabs/h5cpp-compiler.svg)](https://github.com/vargalabs/h5cpp-compiler/releases)
[![Documentation](https://img.shields.io/badge/docs-stable-blue)](https://vargalabs.github.io/h5cpp-compiler)

HDF5 schema compiler for typed C++ data
--------------------------------------
**h5cpp-compiler** is a build-time schema generator for [H5CPP](https://h5cpp.org). It uses LLVM/Clang tooling to inspect C++ types reachable from H5CPP I/O calls such as `h5::create`, `h5::read`, `h5::write`, and `h5::append`. From those types, it emits HDF5 compound-type descriptors, so the C++ source remains the schema source of truth. The compiler is optional: H5CPP and HDF5 can be used without it. It becomes valuable when your object model changes often, or when maintaining hand-written HDF5 compound descriptors would be brittle.


| Layer | Role |
|------|------|
| [H5CPP](https://github.com/vargalabs/h5cpp) | Header-only C++ HDF5 I/O library |
| **h5cpp-compiler** | Build-time generator for C++ compound-type metadata |



## Supported platforms

| OS / Compiler | GCC 13            | GCC 14            | GCC 15    | Clang 17         | Clang 18         | Clang 19         | Clang 20         | Apple Clang    | MSVC           |
|---------------|-------------------|-------------------|-----------|------------------|------------------|------------------|------------------|----------------|----------------|
| Ubuntu 22.04  | ![u22-gcc13][200] | ![NA][NA]         | ![NA][NA] | ![u22-cl17][250] | ![u22-cl18][251] | ![u22-cl19][252] | ![u22-cl20][253] | ![NA][NA]      | ![NA][NA]      |
| Ubuntu 24.04  | ![u24-gcc13][300] | ![u24-gcc14][301] | ![NA][NA] | ![NA][NA]        | ![u24-cl18][351] | ![u24-cl19][352] | ![u24-cl20][353] | ![NA][NA]      | ![NA][NA]      |
| macOS 15      | ![NA][NA]         | ![NA][NA]         | ![NA][NA] | ![NA][NA]        | ![NA][NA]        | ![NA][NA]        | ![NA][NA]        | ![mac-ac][400] | ![NA][NA]      |
| Windows       | ![NA][NA]         | ![NA][NA]         | ![NA][NA] | ![NA][NA]        | ![NA][NA]        | ![NA][NA]        | ![NA][NA]        | ![NA][NA]      | ![win-msvc][500] |

## Release packages
Self-contained, statically linked packages are available for common Linux, macOS, and Windows targets.

| Platform | Package |
|----------|---------|
| Debian / Ubuntu amd64 | [h5cpp-compiler-1.12.3-Linux-amd64.deb](https://github.com/vargalabs/h5cpp-compiler/releases/download/v1.12.3/h5cpp-compiler-1.12.3-Linux-amd64.deb) |
| Debian / Ubuntu arm64 | [h5cpp-compiler-1.12.3-Linux-arm64.deb](https://github.com/vargalabs/h5cpp-compiler/releases/download/v1.12.3/h5cpp-compiler-1.12.3-Linux-arm64.deb) |
| Red Hat / Fedora x86_64 | [h5cpp-compiler-1.12.3-Linux-x86_64.rpm](https://github.com/vargalabs/h5cpp-compiler/releases/download/v1.12.3/h5cpp-compiler-1.12.3-Linux-x86_64.rpm) |
| Red Hat / Fedora aarch64 | [h5cpp-compiler-1.12.3-Linux-aarch64.rpm](https://github.com/vargalabs/h5cpp-compiler/releases/download/v1.12.3/h5cpp-compiler-1.12.3-Linux-aarch64.rpm) |
| macOS arm64 | [h5cpp-compiler-1.12.3-Darwin.pkg](https://github.com/vargalabs/h5cpp-compiler/releases/download/v1.12.3/h5cpp-compiler-1.12.3-Darwin.pkg) |
| Windows x64 | [h5cpp-compiler-1.12.3-win64.exe](https://github.com/vargalabs/h5cpp-compiler/releases/download/v1.12.3/h5cpp-compiler-1.12.3-win64.exe) |


## Under the hood

h5cpp-compiler uses LLVM/Clang tooling to build the AST of your translation unit, locates struct types passed to h5:: I/O operators, walks the reachable C++ value-object graph, and emits a self-contained header with HDF5 `H5T_COMPOUND` descriptors in dependency order. The generated file uses `#pragma once` and drops straight into your build.

## CMake integration

```cmake
find_package(h5cpp-compiler REQUIRED)
h5cpp_compiler_generate(
    INPUT  ${CMAKE_CURRENT_SOURCE_DIR}/some-source.cpp
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/some-header.hpp
)
target_include_directories(my_app PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
target_sources(my_app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/some-source.cpp)
```

## Command-line usage

```bash
h5cpp -o some-header.hpp source.cpp -- $(CXXFLAGS)
```

Output format defaults to `--hdf5`. Pass `--protocol-buffers` to select the Protobuf backend (currently a stub).

```cpp
#include <h5cpp/all>

struct particle_t {
    double x, y, z;
    int id;
};

#include "some-header.hpp"

int main() {
    auto fd = h5::create("storage.h5");
    std::vector<particle_t> particles(100);
    h5::write(fd, "particles", particles);
}
```

```bash
h5cpp -o some-header.hpp experiment.cpp -- -std=c++17 -I/usr/local/HDF_Group/HDF5/2.2.0/include
```

### Generated output
```
#pragma once
#include <hdf5.h>

namespace h5 {
    template<> hid_t inline register_struct<particle_t>(){

        hid_t ct_00 = H5Tcreate(H5T_COMPOUND, sizeof (particle_t));
        H5Tinsert(ct_00, "x",	HOFFSET(particle_t,x),H5T_NATIVE_DOUBLE);
        H5Tinsert(ct_00, "y",	HOFFSET(particle_t,y),H5T_NATIVE_DOUBLE);
        H5Tinsert(ct_00, "z",	HOFFSET(particle_t,z),H5T_NATIVE_DOUBLE);
        H5Tinsert(ct_00, "id",	HOFFSET(particle_t,id),H5T_NATIVE_INT);

        return ct_00;
    };
}
H5CPP_REGISTER_STRUCT(particle_t);

```

```bash
g++ -std=c++17 experiment.cpp -I. -I/usr/local/HDF_Group/HDF5/2.2.0/include \
    -L/usr/local/HDF_Group/HDF5/2.2.0/lib -lhdf5 -lz \
    -Wl,-rpath,/usr/local/HDF_Group/HDF5/2.2.0/lib -o experiment
```
```text
HDF5 "storage.h5" {
    GROUP "/" {
    DATASET "particles" {
        DATATYPE  H5T_COMPOUND {
            H5T_IEEE_F64LE "x";
            H5T_IEEE_F64LE "y";
            H5T_IEEE_F64LE "z";
            H5T_STD_I32LE "id";
        }
        DATASPACE  SIMPLE { ( 100 ) / ( 100 ) }
        STORAGE_LAYOUT {
            CONTIGUOUS
            SIZE 3200
            OFFSET 2048
        }
        FILTERS {
            NONE
        }
        FILLVALUE {
            FILL_TIME H5D_FILL_TIME_IFSET
            VALUE  H5D_FILL_VALUE_DEFAULT
        }
        ALLOCATION_TIME {
            H5D_ALLOC_TIME_LATE
        }
    }
    }
}
```

## Build from source

Source builds require:

- LLVM / Clang development libraries
- CMake 3.14+
- C++17 compiler

```bash
cmake -DCMAKE_BUILD_TYPE=Release -S . -B build
cmake --build build --parallel
sudo cmake --install build
```



See `examples/cmake-integration/` for a complete working project.

## Compatibility

| h5cpp-compiler | h5cpp library |
|---|---|
| 1.12.x | 1.12.x |

Keep the compiler and library versions in sync.

## License

MIT. See [LICENSE](LICENSE) and [LICENSE.LLVM](LICENSE.LLVM).

<!-- Static NA badge — committed once to the repo, never regenerated by CI -->
[NA]: https://vargalabs.github.io/h5cpp-compiler/badges/na.svg

<!-- Ubuntu 22.04 -->
[200]: https://vargalabs.github.io/h5cpp-compiler/badges/ubuntu-22.04-gcc-13.svg
[250]: https://vargalabs.github.io/h5cpp-compiler/badges/ubuntu-22.04-clang-17.svg
[251]: https://vargalabs.github.io/h5cpp-compiler/badges/ubuntu-22.04-clang-18.svg
[252]: https://vargalabs.github.io/h5cpp-compiler/badges/ubuntu-22.04-clang-19.svg
[253]: https://vargalabs.github.io/h5cpp-compiler/badges/ubuntu-22.04-clang-20.svg

<!-- Ubuntu 24.04 -->
[300]: https://vargalabs.github.io/h5cpp-compiler/badges/ubuntu-24.04-gcc-13.svg
[301]: https://vargalabs.github.io/h5cpp-compiler/badges/ubuntu-24.04-gcc-14.svg
[351]: https://vargalabs.github.io/h5cpp-compiler/badges/ubuntu-24.04-clang-18.svg
[352]: https://vargalabs.github.io/h5cpp-compiler/badges/ubuntu-24.04-clang-19.svg
[353]: https://vargalabs.github.io/h5cpp-compiler/badges/ubuntu-24.04-clang-20.svg

<!-- macOS 15 -->
[400]: https://vargalabs.github.io/h5cpp-compiler/badges/macos-15-apple-clang.svg

<!-- Windows -->
[500]: https://vargalabs.github.io/h5cpp-compiler/badges/windows-latest-msvc.svg
