---
hide:
  - toc
---

<div class="flex flex-wrap gap-2">
  <a href="https://github.com/vargaconsulting/h5cpp-compiler/actions">
    <img src="https://img.shields.io/badge/CI-passing-brightgreen" class="h-6" />
  </a>
  <a href="https://codecov.io/gh/vargaconsulting/h5cpp-compiler">
    <img src="https://img.shields.io/codecov/c/github/vargaconsulting/h5cpp-compiler" class="h-6" />
  </a>
  <a href="LICENSE">
    <img src="https://img.shields.io/badge/license-MIT-green.svg" class="h-6" />
  </a>
  <a href="https://doi.org/10.5281/zenodo.17069343">
    <img src="https://zenodo.org/badge/DOI/10.5281/zenodo.17069343.svg" class="h-6" />
  </a>
</div>


| OS / Compiler | GCC 13      | GCC 14      | GCC 15      | Clang 14      | Clang 15      | Clang 17      |
|---------------|-------------|-------------|-------------|---------------|---------------|---------------|
| Ubuntu 22.04  |![gcc13][200]|![gcc14][201]|![gcc15][202]|![clang14][210]|![clang15][211]|![clang17][212]|
| Ubuntu 24.04  |![gcc13][300]|![gcc14][301]|![gcc15][302]|![clang14][310]|![clang15][311]|![clang17][312]|
| macOS         |![gcc13][600]|![gcc14][601]|![gcc15][602]|![clang14][610]|![clang15][611]|![clang17][612]|
| Windows       |![gcc13][700]|![gcc13][701]|![gcc13][702]|![clang14][710]|![clang15][711]|![clang17][712]|

H5CPP compiler is an LLVM/Clang–powered **reverse schema compiler**. Instead of forcing you to design schemas up front, the H5CPP compiler dives into your existing C and C++ code, extracts full type graphs, and automatically generates persistence descriptors. Born out of the [H5CPP header-only library](http://h5cpp.org), the compiler eliminates the grind of hand-crafting shim code for HDF5 compound datatypes. Mark a variable with a persistence operator (like `h5::write`), and the H5CPP compiler will walk the AST, discover every dependent type, and in clean topological order emit a ready-to-use header with HDF5 descriptors. Self-contained, include-guarded, and built to drop straight into your workflow.  

The payoff? **Non-intrusive, reflection-driven persistence for modern C and C++** — the ease of Python or Java–style serialization, but with the raw performance, type safety, and zero-overhead ethos that C and C++ demand. Complex POD structs? Deeply nested arrays? STL containers? No problem. The H5CPP compiler handles them with ease. Today it supports `std::vector`, tomorrow an ever-growing slice of the STL universe.  

```cpp
std::vector<sn::example::Record> vec 
    = h5::utils::get_test_data<sn::example::Record>(20);

// mark vec with an h5:: operator and delegate 
// the details to h5cpp compiler
h5::write(fd, "orm/partial/vector one_shot", vec );
````

### Example Structs

```cpp
namespace sn {
    namespace typecheck {
        struct Record {
            char  _char; unsigned char _uchar; short _short; unsigned short _ushort;
            int _int; unsigned int _uint; long _long; unsigned long _ulong;
            long long int _llong; unsigned long long _ullong;
            float _float; double _double; long double _ldouble;
            bool _bool;
            // wide characters are not supported in HDF5
            // wchar_t _wchar; char16_t _wchar16; char32_t _wchar32;
        };
    }

    namespace other {
        struct Record {
            MyUInt idx;
            MyUInt aa;
            double field_02[3];
            typecheck::Record field_03[4];
        };
    }

    namespace example {
        struct Record {
            MyUInt idx;
            float field_02[7];
            sn::other::Record field_03[5];
            sn::other::Record field_04[5]; // optimized out, same as previous
            other::Record field_05[3][8];  // array of arrays
        };
    }

    namespace not_supported_yet {
        // NON POD: not supported in phase 1
        struct Container {
            double idx;
            std::string field_05;                  // non-POD
            std::vector<example::Record> field_02; // non-POD
        };
    }

    /* BEGIN IGNORED STRUCT */
    // These structs are not referenced with h5::read|h5::write|h5::create
    struct IgnoredRecord {
        signed long int idx;
        float field_0n;
    };
    /* END IGNORED STRUCT */
}
```

<!-- Ubuntu 22.04 -->
[200]: https://vargaconsulting.github.io/h5cpp-compiler/badges/ubuntu-22.04-gcc-13.svg
[201]: https://vargaconsulting.github.io/h5cpp-compiler/badges/ubuntu-22.04-gcc-14.svg
[202]: https://vargaconsulting.github.io/h5cpp-compiler/badges/ubuntu-22.04-gcc-15.svg
[210]: https://vargaconsulting.github.io/h5cpp-compiler/badges/ubuntu-22.04-clang-14.svg
[211]: https://vargaconsulting.github.io/h5cpp-compiler/badges/ubuntu-22.04-clang-15.svg
[212]: https://vargaconsulting.github.io/h5cpp-compiler/badges/ubuntu-22.04-clang-17.svg

<!-- Ubuntu 24.04 -->
[300]: https://vargaconsulting.github.io/h5cpp-compiler/badges/ubuntu-24.04-gcc-13.svg
[301]: https://vargaconsulting.github.io/h5cpp-compiler/badges/ubuntu-24.04-gcc-14.svg
[302]: https://vargaconsulting.github.io/h5cpp-compiler/badges/ubuntu-24.04-gcc-15.svg
[310]: https://vargaconsulting.github.io/h5cpp-compiler/badges/ubuntu-24.04-clang-14.svg
[311]: https://vargaconsulting.github.io/h5cpp-compiler/badges/ubuntu-24.04-clang-15.svg
[312]: https://vargaconsulting.github.io/h5cpp-compiler/badges/ubuntu-24.04-clang-17.svg

<!-- macOS latest -->
[600]: https://vargaconsulting.github.io/h5cpp-compiler/badges/macos-latest-gcc-13.svg
[601]: https://vargaconsulting.github.io/h5cpp-compiler/badges/macos-latest-gcc-14.svg
[602]: https://vargaconsulting.github.io/h5cpp-compiler/badges/macos-latest-gcc-15.svg
[610]: https://vargaconsulting.github.io/h5cpp-compiler/badges/macos-latest-clang-14.svg
[611]: https://vargaconsulting.github.io/h5cpp-compiler/badges/macos-latest-clang-15.svg
[612]: https://vargaconsulting.github.io/h5cpp-compiler/badges/macos-latest-clang-17.svg

<!-- Windows (clang & gcc) -->
[700]: https://vargaconsulting.github.io/h5cpp-compiler/badges/windows-latest-gcc-13.svg
[701]: https://vargaconsulting.github.io/h5cpp-compiler/badges/windows-latest-gcc-14.svg
[702]: https://vargaconsulting.github.io/h5cpp-compiler/badges/windows-latest-gcc-15.svg
[710]: https://vargaconsulting.github.io/h5cpp-compiler/badges/windows-latest-clang-14.svg
[711]: https://vargaconsulting.github.io/h5cpp-compiler/badges/windows-latest-clang-15.svg
[712]: https://vargaconsulting.github.io/h5cpp-compiler/badges/windows-latest-clang-17.svg
