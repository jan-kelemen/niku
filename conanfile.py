from conan import ConanFile

from conan.tools.cmake import CMake, CMakeDeps, CMakeToolchain, cmake_layout
from conan.tools.files import copy, get, rmdir

import os

require_conan_version = ">=2.0"

class NikuConan(ConanFile):
    name = "niku"
    settings = "os", "compiler", "build_type", "arch"
    version = "0.1"

    options = {
        "shared": [True, False],
        "fPIC": [True, False],
        "develop": [True, False],
    }

    default_options = {
        "shared": False,
        "fPIC": True,
        "develop": False,
    }

    exports_sources = "cmake/*", "src/*", "vendor/*", "CMakeLists.txt", "LICENSE"

    implements = ["auto_shared_fpic"]

    def requirements(self):
        self.requires("angelscript/2.37.0")
        self.requires("boost/1.87.0", transitive_headers=False)
        self.requires("fastgltf/0.8.0", transitive_headers=False)
        self.requires("fmt/11.1.4")
        self.requires("freetype/2.13.3")
        self.requires("glm/1.0.1")
        self.requires("glslang/1.4.304.0", transitive_headers=False)
        self.requires("harfbuzz/11.2.0")
        self.requires("imgui/1.91.9b-docking")
        self.requires("joltphysics/5.3.0")
        self.requires("mikktspace/cci.20200325", transitive_headers=False)
        self.requires("meshoptimizer/0.23", transitive_headers=False)
        self.requires("sdl/3.2.8")
        self.requires("spdlog/1.15.2", transitive_headers=False)
        self.requires("spirv-cross/1.4.304.0", transitive_headers=False)
        self.requires("stb/cci.20240531", transitive_headers=False)
        self.requires("volk/1.4.304.0")
        self.requires("vulkan-memory-allocator/3.2.1")

        if self.options.develop:
            self.requires("entt/3.15.0")
            self.requires("recastnavigation/1.6.0")
            self.requires("simdjson/3.12.2")
            self.requires("tree-sitter/0.25.3")
            self.requires("tree-sitter-glsl/0.2.0")

    def build_requirements(self):
        self.tool_requires("cmake/[^3.27]")
        self.test_requires("catch2/3.8.0")

    def layout(self):
        cmake_layout(self)

    def generate(self):
        tc = CMakeToolchain(self)

        if not self.options.develop:
            tc.cache_variables["NIKU_BUILD_DEMOS"] = False

        tc.user_presets_path = "ConanPresets.json"
        tc.generate()

        cmake = CMakeDeps(self)
        cmake.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        copy(self, "LICENSE", src=self.source_folder, dst=os.path.join(self.package_folder, "licenses"))
        cmake = CMake(self)
        cmake.install()
        rmdir(self, os.path.join(self.package_folder, "lib", "cmake"))

    def package_info(self):
        component = "vma_impl"
        self.cpp_info.components[component].set_property("cmake_target_name", f"niku::{component}")
        self.cpp_info.components[component].defines = ["VK_NO_PROTOTYPES"]
        self.cpp_info.components[component].libs = [component]
        self.cpp_info.components[component].requires.extend(["vulkan-memory-allocator::vulkan-memory-allocator"])

        component = "stb_impl"
        self.cpp_info.components[component].set_property("cmake_target_name", f"niku::{component}")
        self.cpp_info.components[component].libs = [component]
        self.cpp_info.components[component].requires.extend(["stb::stb"])

        component = "imgui_vulkan_impl"
        self.cpp_info.components[component].set_property("cmake_target_name", f"niku::{component}")
        self.cpp_info.components[component].defines = ["IMGUI_IMPL_VULKAN_USE_VOLK", "VK_NO_PROTOTYPES"]
        self.cpp_info.components[component].libs = [component]
        self.cpp_info.components[component].requires.extend(["imgui::imgui", "volk::volk"])

        component = "imgui_sdl_impl"
        self.cpp_info.components[component].set_property("cmake_target_name", f"niku::{component}")
        self.cpp_info.components[component].libs = [component]
        self.cpp_info.components[component].requires.extend(["imgui::imgui", "sdl::sdl"])

        component = "glm_impl"
        self.cpp_info.components[component].set_property("cmake_target_name", f"niku::{component}")
        self.cpp_info.components[component].defines = ["IMGUI_IMPL_VULKAN_USE_VOLK", "VK_NO_PROTOTYPES"]
        self.cpp_info.components[component].requires.extend(["glm::glm"])

        component = "as_scriptbuilder"
        self.cpp_info.components[component].set_property("cmake_target_name", f"niku::{component}")
        self.cpp_info.components[component].libs = [component]
        self.cpp_info.components[component].requires.extend(["angelscript::angelscript"])

        component = "as_scriptarray"
        self.cpp_info.components[component].set_property("cmake_target_name", f"niku::{component}")
        self.cpp_info.components[component].libs = [component]
        self.cpp_info.components[component].requires.extend(["angelscript::angelscript"])

        component = "as_scriptstdstring"
        self.cpp_info.components[component].set_property("cmake_target_name", f"niku::{component}")
        self.cpp_info.components[component].libs = [component]
        self.cpp_info.components[component].requires.extend(["angelscript::angelscript"])

        component = "cppext"
        self.cpp_info.components[component].set_property("cmake_target_name", f"niku::{component}")

        component = "vkrndr"
        self.cpp_info.components[component].set_property("cmake_target_name", f"niku::{component}")
        self.cpp_info.components[component].defines = ["VK_NO_PROTOTYPES"]
        if self.settings.build_type in ["Debug", "RelWithDebInfo"]:
            self.cpp_info.components[component].defines = ["VKRNDR_ENABLE_DEBUG_UTILS=1"]

        self.cpp_info.components[component].libs = [component]
        self.cpp_info.components[component].requires.extend(["cppext", "vma_impl"])
        self.cpp_info.components[component].requires.extend(["volk::volk", "boost::headers", "fmt::fmt", "spdlog::spdlog"])

        component = "vkglsl"
        self.cpp_info.components[component].set_property("cmake_target_name", f"niku::{component}")
        self.cpp_info.components[component].libs = [component]
        self.cpp_info.components[component].requires.extend(["vkrndr"])
        self.cpp_info.components[component].requires.extend([ "glslang::glslang", "glslang::glslang-default-resource-limits", "glslang::spirv",  "spirv-cross::spirv-cross"])

        component = "ngnast"
        self.cpp_info.components[component].set_property("cmake_target_name", f"niku::{component}")
        self.cpp_info.components[component].libs = [component]
        self.cpp_info.components[component].requires.extend(["vkrndr", "glm_impl", "cppext", "stb_impl"])
        self.cpp_info.components[component].requires.extend(["boost::headers", "fastgltf::fastgltf", "mikktspace::mikktspace", "meshoptimizer::meshoptimizer", "spdlog::spdlog"])
        
        component = "ngnscr"
        self.cpp_info.components[component].set_property("cmake_target_name", f"niku::{component}")
        self.cpp_info.components[component].libs = [component]
        self.cpp_info.components[component].requires.extend(["cppext", "as_scriptbuilder", "as_scriptarray", "as_scriptstdstring"])
        self.cpp_info.components[component].requires.extend(["angelscript::angelscript", "spdlog::spdlog"])

        component = "ngntxt"
        self.cpp_info.components[component].set_property("cmake_target_name", f"niku::{component}")
        self.cpp_info.components[component].libs = [component]
        self.cpp_info.components[component].requires.extend(["vkrndr", "cppext"])
        self.cpp_info.components[component].requires.extend(["Freetype::Freetype", "harfbuzz::harfbuzz", "spdlog::spdlog"])

        component = "ngnphy"
        self.cpp_info.components[component].set_property("cmake_target_name", f"niku::{component}")
        self.cpp_info.components[component].libs = [component]
        self.cpp_info.components[component].requires.extend(["cppext", "glm_impl"])
        self.cpp_info.components[component].requires.extend(["joltphysics::joltphysics"])

        component = "ngngfx"
        self.cpp_info.components[component].set_property("cmake_target_name", f"niku::{component}")
        self.cpp_info.components[component].libs = [component]
        self.cpp_info.components[component].requires.extend(["glm_impl", "vkrndr"])
