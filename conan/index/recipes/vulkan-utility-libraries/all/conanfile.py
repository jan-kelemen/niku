from conan import ConanFile
from conan.tools.cmake import CMake, CMakeDeps, CMakeToolchain, cmake_layout
from conan.tools.files import copy, get, replace_in_file

import os

required_conan_version = ">2.0"


class VulkanUtilityLibrariesConan(ConanFile):
    name = "vulkan-utility-libraries"
    description = "Vulkan-Utility-Libraries"
    license = "Apache-2.0"
    topics = ("vulkan-headers", "vulkan")
    homepage = "https://github.com/KhronosGroup/Vulkan-Utility-Libraries"
    package_type = "library"
    settings = "os", "arch", "compiler", "build_type"
    options = {
        "shared": [True, False],
        "fPIC": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
    }
    implements = ["auto_shared_fpic"]
    def layout(self):
        cmake_layout(self)
        
    def requirements(self):
        self.requires(f"vulkan-headers/{self.version}", transitive_headers=True)
        
    def source(self):
        get(self, **self.conan_data["sources"][self.version], strip_root=True)
        
    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["BUILD_TESTS"] = False
        tc.generate()
        deps = CMakeDeps(self)
        deps.generate()
        
    def _patch_sources(self):
        replace_in_file(self, os.path.join(self.source_folder, "CMakeLists.txt"),
                              'set(CMAKE_CXX_STANDARD_REQUIRED ON)', "")
        replace_in_file(self, os.path.join(self.source_folder, "CMakeLists.txt"),
                              'set(CMAKE_CXX_EXTENSIONS OFF)', "")
        replace_in_file(self, os.path.join(self.source_folder, "CMakeLists.txt"),
                              'set(CMAKE_CXX_VISIBILITY_PRESET "hidden")', "")
        replace_in_file(self, os.path.join(self.source_folder, "CMakeLists.txt"),
                              'set(CMAKE_C_VISIBILITY_PRESET "hidden")', "")
        replace_in_file(self, os.path.join(self.source_folder, "CMakeLists.txt"),
                              'set(CMAKE_VISIBILITY_INLINES_HIDDEN "YES")', "")
        replace_in_file(self, os.path.join(self.source_folder, "CMakeLists.txt"),
                              'set(CMAKE_CXX_STANDARD 17)', "")

    def build(self):
        self._patch_sources()
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        copy(self, "LICENSE*", src=self.source_folder, dst=os.path.join(self.package_folder, "licenses"))
        copy(self, "*", src=os.path.join(self.source_folder, "include"), dst=os.path.join(self.package_folder, "include"))

    def package_info(self):
        self.cpp_info.set_property("cmake_file_name", "VulkanUtilityLibraries")
        self.cpp_info.components["VulkanUtilityHeaders"].set_property("cmake_target_name", "Vulkan::UtilityHeaders")
        self.cpp_info.components["VulkanUtilityHeaders"].bindirs = []
        self.cpp_info.components["VulkanUtilityHeaders"].libdirs = []
        self.cpp_info.components["VulkanUtilityHeaders"].requires.extend(["vulkan-headers::vulkan-headers"])

