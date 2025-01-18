from conan import ConanFile
from conan.tools.cmake import CMake, CMakeDeps, CMakeToolchain, cmake_layout

require_conan_version = ">=2.0"

class NikuConan(ConanFile):
    name = "niku"
    settings = "os", "compiler", "build_type", "arch"
    version = "0.1"

    exports_sources = "cmake", "src", "CMakeLists.txt", "LICENSE"

    def requirements(self):
        self.requires("angelscript/2.37.0")
        self.requires("boost/1.86.0")
        self.requires("entt/3.14.0")
        self.requires("fastgltf/0.8.0")
        self.requires("fmt/11.1.1")
        self.requires("freetype/2.13.3")
        self.requires("glm/1.0.1")
        self.requires("glslang/1.4.304.0")
        self.requires("imgui/1.91.6-docking")
        self.requires("joltphysics/5.2.0")
        self.requires("meshoptimizer/0.22")
        self.requires("perlinnoise/3.0.0")
        self.requires("sdl/2.30.11")
        self.requires("simdjson/3.11.5")
        self.requires("spdlog/1.15.x")
        self.requires("spirv-cross/1.4.304.0")
        self.requires("stb/cci.20240531")
        self.requires("volk/1.4.304.0")
        self.requires("vulkan-memory-allocator/3.2.0")

    def build_requirements(self):
        self.tool_requires("cmake/[^3.27]")
        self.test_requires("catch2/3.8.0")

    def layout(self):
        cmake_layout(self)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.user_presets_path = "ConanPresets.json"
        tc.generate()

        cmake = CMakeDeps(self)
        cmake.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

