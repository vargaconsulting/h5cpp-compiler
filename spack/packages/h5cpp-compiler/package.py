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

    version("1.12.6", sha256="0000000000000000000000000000000000000000000000000000000000000000")
    version("1.12.5", sha256="0000000000000000000000000000000000000000000000000000000000000000")
    version("1.12.4", sha256="0000000000000000000000000000000000000000000000000000000000000000")
    version("1.12.3", sha256="0000000000000000000000000000000000000000000000000000000000000000")

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
