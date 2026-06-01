from conan import ConanFile
from conan.tools.cmake import CMake, CMakeDeps, CMakeToolchain, cmake_layout
from conan.tools.files import copy


class H5CppCompilerConan(ConanFile):
    name = "h5cpp-compiler"
    version = "1.12.6"
    package_type = "application"
    settings = "os", "compiler", "build_type", "arch"
    exports_sources = (
        "CMakeLists.txt",
        "src/*",
        "cmake/*",
        "h5cpp.1",
        "COPYRIGHT*",
        "COPYRIGHT.txt",
    )

    def layout(self):
        cmake_layout(self)

    def requirements(self):
        # h5cpp-compiler is compatible with LLVM 18+, 19+, and 20.
        # ConanCenter currently provides llvm-core at 19.1.7.
        self.requires("llvm-core/19.1.7")

    def generate(self):
        deps = CMakeDeps(self)
        deps.generate()
        tc = CMakeToolchain(self)
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        cmake = CMake(self)
        cmake.install()

    def package_info(self):
        self.cpp_info.bindirs = ["bin"]
        self.cpp_info.libdirs = []
        self.cpp_info.includedirs = []

    def package_id(self):
        # For a build-time tool, compiler and build_type do not affect the
        # package ID in a meaningful way for consumers that use it as a
        # tool_requires.  Clearing them keeps the package ID stable.
        del self.info.settings.compiler
        del self.info.settings.build_type
