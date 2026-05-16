[![CI](https://github.com/vargalabs/h5cpp-compiler/actions/workflows/ci.yml/badge.svg)](https://github.com/vargalabs/h5cpp-compiler/actions/workflows/ci.yml)
[![codecov](https://codecov.io/gh/vargalabs/h5cpp-compiler/branch/release/graph/badge.svg)](https://app.codecov.io/gh/vargalabs/h5cpp-compiler/tree/release)
[![MIT License](https://img.shields.io/badge/license-MIT-green.svg)](LICENSE)
[![DOI](https://zenodo.org/badge/DOI/10.5281/zenodo.17069343.svg)](https://doi.org/10.5281/zenodo.17069343)
[![GitHub release](https://img.shields.io/github/v/release/vargalabs/h5cpp-compiler.svg)](https://github.com/vargalabs/h5cpp-compiler/releases)
[![Documentation](https://img.shields.io/badge/docs-stable-blue)](https://vargalabs.github.io/h5cpp-compiler)

# h5cpp-compiler

> h5cpp-compiler keeps your HDF5 schemas in sync with your C++ structs — automatically, at build time, before silent corruption becomes a runtime bug.

Schema drift is a data-corruption event waiting to happen. This compiler catches it at build time.

## Do I need this?

| Your situation | What to use |
|---|---|
| Simple POD structs, no nesting, no arrays | [h5cpp header-only](https://h5cpp.org) — manual registration works |
| Nested structs, C-style arrays, namespaced types, `std::vector<T>` | **h5cpp-compiler** |

## The problem

You changed a field in your simulation struct. Recompiled. Ran the job for six hours. The output file looks fine — until you load it and the particle coordinates are in the temperature column.

HDF5 compound types and C++ structs must match byte-for-byte. One padding change, one field reorder, one `int` → `double` swap, and you're writing garbage. The worst part? You usually don't know until post-processing, three days later, when you can't reproduce the run.

## The solution

h5cpp-compiler is a build-time correctness gate. It parses your translation unit, finds every POD struct referenced by `h5::write`, `h5::read`, `h5::create`, or `h5::append`, and emits HDF5 compound-type descriptors that match your C++ layout exactly.

Change a struct → rebuild → the generated descriptor tracks the change immediately. No silent mismatches. No 3 AM debugging sessions.

## 30-second demo

```bash
# 1. A struct marked with an h5:: operator
cat > experiment.cpp << 'EOF'
#include <h5cpp/all>
struct Particle { double x, y, z; int id; };
int main() {
    auto fd = h5::create("run.h5");
    std::vector<Particle> particles(100);
    h5::write(fd, "particles", particles);
}
EOF

# 2. Generate descriptors that match the struct exactly
h5cpp experiment.cpp -- $(CXXFLAGS) -Dgenerated.h

# 3. Someone refactors the struct — field moves, padding shifts
cat > experiment.cpp << 'EOF'
#include <h5cpp/all>
struct Particle { double x, y; int id; double z; };  // z moved
int main() {
    auto fd = h5::open("run.h5");
    std::vector<Particle> particles(100);
    h5::write(fd, "particles", particles);
}
EOF

# 4. Re-generate — descriptor now reflects the new layout
h5cpp experiment.cpp -- $(CXXFLAGS) -Dgenerated.h
# The generated descriptor matches your C++ struct exactly.
# If it no longer matches the existing file, HDF5 errors out instead
# of silently corrupting the dataset.
```

## Installation

### Prerequisites

- LLVM / Clang development libraries
- CMake 3.14+
- C++17 compiler

### Build from source

```bash
# Ubuntu / Debian
sudo apt install build-essential cmake llvm-dev libclang-dev

# macOS
brew install llvm cmake

# Build and install
cmake -DCMAKE_BUILD_TYPE=Release -S . -B build
cmake --build build --parallel
sudo cmake --install build
```

### Prebuilt binaries

See [GitHub Releases](https://github.com/vargalabs/h5cpp-compiler/releases).

## CMake integration

```cmake
find_package(h5cpp-compiler REQUIRED)

h5cpp_compiler_generate(
    INPUT  ${CMAKE_CURRENT_SOURCE_DIR}/experiment.cpp
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/experiment_h5.hpp
)

target_sources(my_app PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/experiment_h5.hpp)
```

See `examples/cmake-integration/` for a complete working project.

## Compatibility

| h5cpp-compiler | h5cpp library |
|---|---|
| 1.10.4.6 | 1.10.4.x |

Keep the compiler and library versions in sync.

## Build matrix

| OS / Compiler | GCC 13            | GCC 14            | GCC 15    | Clang 17         | Clang 18         | Clang 19         | Clang 20         | Apple Clang    | MSVC           |
|---------------|-------------------|-------------------|-----------|------------------|------------------|------------------|------------------|----------------|----------------|
| Ubuntu 22.04  | ![u22-gcc13][200] | ![NA][NA]         | ![NA][NA] | ![u22-cl17][250] | ![u22-cl18][251] | ![u22-cl19][252] | ![u22-cl20][253] | ![NA][NA]      | ![NA][NA]      |
| Ubuntu 24.04  | ![u24-gcc13][300] | ![u24-gcc14][301] | ![NA][NA] | ![NA][NA]        | ![u24-cl18][351] | ![u24-cl19][352] | ![u24-cl20][353] | ![NA][NA]      | ![NA][NA]      |
| macOS 15      | ![NA][NA]         | ![NA][NA]         | ![NA][NA] | ![NA][NA]        | ![NA][NA]        | ![NA][NA]        | ![NA][NA]        | ![mac-ac][400] | ![NA][NA]      |
| Windows       | ![NA][NA]         | ![NA][NA]         | ![NA][NA] | ![NA][NA]        | ![NA][NA]        | ![NA][NA]        | ![NA][NA]        | ![NA][NA]      | ![win-msvc][500] |

## How it works

h5cpp-compiler uses LLVM/Clang tooling to build the AST of your translation unit, locates struct types passed to h5:: I/O operators, and emits a self-contained header with HDF5 `H5T_COMPOUND` descriptors in topological order. The generated file uses `#pragma once` and drops straight into your build.

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
