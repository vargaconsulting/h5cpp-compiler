<!---
 Copyright (c) 2018-2020 Steven Varga, Toronto,ON Canada
 Author: Varga, Steven <steven@vargaconsulting.ca>
--->

Source code transformation tool for HDF5 dataformat  H5CPP header only library  
----------------------------------------------------------------------------------------------------
## Build Matrix

| OS / Compiler | GCC 13      | GCC 14      | GCC 15      | Clang 14      | Clang 15      | Clang 17      |
|---------------|-------------|-------------|-------------|---------------|---------------|---------------|
| Ubuntu 22.04  |![gcc13][200]|![gcc14][201]|![gcc15][202]|![clang14][210]|![clang15][211]|![clang17][212]|
| Ubuntu 24.04  |![gcc13][300]|![gcc14][301]|![gcc15][302]|![clang14][310]|![clang15][311]|![clang17][312]|
| macOS         |![gcc13][600]|![gcc14][601]|![gcc15][602]|![clang14][610]|![clang15][611]|![clang17][612]|
| Windows       |![gcc13][700]|![gcc13][701]|![gcc13][702]|![clang14][710]|![clang15][711]|![clang17][712]|


This source code transformation tool simplifies the otherwise time consuming process of generating the shim code for HDF5 Compound datatypes by building the AST of a given TU translation unit, and identifying all POD datatypes referenced from H5CPP operators/functions.
The result is a seamless persistence much similar to python, java or other reflection based languages. 

The following excerpt shows the mechanism, how `vec` variable is marked by `h5::write` operator. When `h5cpp` tool is invoked it builds the full AST of the translation unit, finds the referenced types, then in topological order generates HDF5 COMPOUND datatype descriptors. The generated file has include guards, and meant to be used with [H5CPP template library](h5cpp.org). POD struct types may be arbitrary deep, embedded in POD C like arrays, and may be referenced from STL containers. Currently `stl::vector` is supported, but in time full support will be provided.

```cpp
...
std::vector<sn::example::Record> vec 
    = h5::utils::get_test_data<sn::example::Record>(20);
// mark vec  with an h5:: operator and delegate 
// the details to h5cpp compiler
h5::write(fd, "orm/partial/vector one_shot", vec );
...

// some include files with complex POD types, embedded in arbitrary name space
namespace sn {
	namespace typecheck {
		struct Record { /*the types with direct mapping to HDF5*/
			char  _char; unsigned char _uchar; short _short; unsigned short _ushort; int _int; unsigned int _uint;
			long _long; unsigned long _ulong; long long int _llong; unsigned long long _ullong;
			float _float; double _double; long double _ldouble;
			bool _bool;
			// wide characters are not supported in HDF5
			// wchar_t _wchar; char16_t _wchar16; char32_t _wchar32;
		};
	}
	namespace other {
		struct Record {                    // POD struct with nested namespace
			MyUInt                    idx; // typedef type 
			MyUInt                     aa; // typedef type 
			double            field_02[3]; // const array mapped 
			typecheck::Record field_03[4]; //
		};
	}
	namespace example {
		struct Record {                    // POD struct with nested namespace
			MyUInt                    idx; // typedef type 
			float             field_02[7]; // const array mapped 
			sn::other::Record field_03[5]; // embedded Record
			sn::other::Record field_04[5]; // must be optimized out, same as previous
			other::Record  field_05[3][8]; // array of arrays 
		};
	}
	namespace not_supported_yet {
		// NON POD: not supported in phase 1
		// C++ Class -> PODstruct -> persistence[ HDF5 | ??? ] -> PODstruct -> C++ Class 
		struct Container {
			double                            idx; // 
			std::string                  field_05; // c++ object makes it non-POD
			std::vector<example::Record> field_02; // ditto
		};
	}
	/* BEGIN IGNORED STRUCT */
	// these structs are not referenced with h5::read|h5::write|h5::create operators
	// hence compiler should ignore them.
	struct IgnoredRecord {
		signed long int   idx;
		float        field_0n;
	};
	/* END IGNORED STRUCTS */
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
