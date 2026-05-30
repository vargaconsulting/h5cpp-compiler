# h5cpp-compiler Conan 2.x Recipe

This directory contains the Conan 2.x recipe for `h5cpp-compiler`.

## Creating the Package

From the repository root:

```bash
conan create .
```

## Consuming as a Tool Dependency

Add `h5cpp-compiler` to your consumer's `tool_requires`:

```python
from conan import ConanFile

class MyProject(ConanFile):
    name = "my-project"
    version = "1.0.0"
    settings = "os", "compiler", "build_type", "arch"
    tool_requires = "h5cpp-compiler/1.12.6"
```

In your `CMakeLists.txt`:

```cmake
find_package(H5CPPCompiler REQUIRED)
h5cpp_compiler_generate(
    INPUT  ${CMAKE_CURRENT_SOURCE_DIR}/particle.cpp
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/particle_h5.hpp
)
target_sources(my_app PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/particle_h5.hpp)
```

## Notes

- The recipe currently depends on `llvm-core/19.1.7` from ConanCenter.
- LLVM 20 support is planned; update the requirement in `conanfile.py` once `llvm-core/20.x` is published.
