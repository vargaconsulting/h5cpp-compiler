# Spack Package for h5cpp-compiler

This directory contains a custom Spack repository with the `h5cpp-compiler` package recipe.

## Adding the repository

```bash
spack repo add /path/to/h5cpp-compiler/spack
```

## Installing the package

```bash
spack install h5cpp-compiler
```

## Notes

- The recipe depends on `llvm +clang` because h5cpp-compiler uses Clang LibTooling.
- CMake 3.14+ and Ninja are required at build time.
- Tests are disabled during the Spack build (`H5CPP_BUILD_TESTS=OFF`).
