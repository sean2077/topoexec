# Draft Conan recipe for review. It is not published to a remote yet.

from conan import ConanFile
from conan.tools.cmake import CMake, CMakeDeps, CMakeToolchain, cmake_layout


class TopoExecConan(ConanFile):
    name = "topoexec"
    version = "0.1.0"
    license = "MIT"
    homepage = "https://github.com/sean2077/topoexec"
    description = "Small embeddable C++20 semantic graph runtime"
    package_type = "library"
    settings = "os", "compiler", "build_type", "arch"
    options = {
        "yaml": [True, False],
        "cli": [True, False],
        "examples": [True, False],
    }
    default_options = {
        "yaml": True,
        "cli": True,
        "examples": False,
    }
    exports_sources = "CMakeLists.txt", "cmake/*", "include/*", "src/*", "tools/*", "schema/*", "LICENSE"

    def config_options(self):
        if self.options.cli:
            self.options.yaml = True

    def requirements(self):
        if self.options.yaml:
            self.requires("yaml-cpp/[>=0.8 <1]")
            self.requires("nlohmann_json/[>=3.11 <4]")
        if self.options.cli:
            self.requires("cli11/[>=2.4 <3]")

    def layout(self):
        cmake_layout(self)

    def generate(self):
        deps = CMakeDeps(self)
        deps.generate()
        tc = CMakeToolchain(self)
        tc.variables["TOPOEXEC_BUILD_YAML"] = bool(self.options.yaml)
        tc.variables["TOPOEXEC_BUILD_CLI"] = bool(self.options.cli)
        tc.variables["TOPOEXEC_BUILD_EXAMPLES"] = bool(self.options.examples)
        tc.variables["TOPOEXEC_BUILD_TESTING"] = False
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        cmake = CMake(self)
        cmake.install()

    def package_info(self):
        self.cpp_info.set_property("cmake_file_name", "topoexec")
        self.cpp_info.set_property("cmake_target_name", "topoexec::runtime")
        self.cpp_info.components["runtime"].set_property("cmake_target_name", "topoexec::runtime")
        self.cpp_info.components["runtime"].libs = ["topoexec_runtime"]
        if self.options.yaml:
            self.cpp_info.components["yaml"].set_property("cmake_target_name", "topoexec::yaml")
            self.cpp_info.components["yaml"].libs = ["topoexec"]
            self.cpp_info.components["yaml"].requires = ["runtime", "yaml-cpp::yaml-cpp", "nlohmann_json::nlohmann_json"]
