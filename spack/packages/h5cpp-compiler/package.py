# Copyright 2013-2024 Lawrence Livermore National Security, LLC and other
# Spack Project Developers. See the top-level COPYRIGHT file for details.
#
# SPDX-License-Identifier: (Apache-2.0 OR MIT)

from spack.package import *


class H5cppCompiler(CMakePackage):
    """h5cpp-compiler is a CLI build-time tool that uses LLVM/Clang LibTooling
    to generate HDF5 C++ bindings from annotated source code.
    """

    homepage = "https://github.com/vargalabs/h5cpp-compiler"
    git = "https://github.com/vargalabs/h5cpp-compiler.git"
    url = "https://github.com/vargalabs/h5cpp-compiler/archive/refs/tags/v1.12.6.tar.gz"

    maintainers("steven-varga")

    license("MIT", checked_by="steven-varga")

    version("1.12.6", sha256="c5f5829cec4908ad93647c17f27d0a82b75a5413fcad8c99232ba2034d0149a2")
    version("1.12.5", sha256="f8990bcbfe90a037b66535c36d6e06e7451491c4cdc9c6fae4dfc15d1b3812be")
    version("1.12.4", sha256="77461c9a482c5460a246724a479c803b011a6f95b94801493816d2c3ca70c732")
    version("1.12.3", sha256="d7eefa490746b361e83df166f50ea9133919224e5e81f893ad4e41176d310371")

    depends_on("cmake@3.14:", type="build")
    depends_on("ninja", type="build")

    # The project targets LLVM 20 and requires the Clang component (LibTooling).
    # Spack's llvm package provides clang via the +clang variant.
    depends_on("llvm +clang")

    def cmake_args(self):
        args = [
            self.define("H5CPP_BUILD_TESTS", "OFF"),
        ]
        return args
