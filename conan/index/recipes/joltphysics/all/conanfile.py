from conan import ConanFile
from conan.tools.build import check_min_cppstd
from conan.tools.cmake import CMake, CMakeToolchain, cmake_layout
from conan.tools.files import copy, get, replace_in_file, rmdir, rm
from conan.tools.microsoft import is_msvc, is_msvc_static_runtime
import os

required_conan_version = ">2.0"


class JoltPhysicsConan(ConanFile):
    name = "joltphysics"
    description = (
        "A multi core friendly rigid body physics and collision detection "
        "library, written in C++, suitable for games and VR applications."
    )
    license = "MIT"
    topics = ("physics", "simulation", "physics-engine", "physics-simulation", "rigid-body", "game", "collision")
    homepage = "https://github.com/jrouwe/JoltPhysics"
    url = "https://github.com/conan-io/conan-center-index"

    package_type = "library"
    settings = "os", "arch", "compiler", "build_type"
    options = {
        "shared": [True, False],
        "fPIC": [True, False],
        "debug_renderer": [True, False],
        "profiler": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
        "debug_renderer": True,
        "profiler": False,
    }
    implements = ["auto_shared_fpic"]

    def layout(self):
        cmake_layout(self, src_folder="src")

    def build_requirements(self):
        self.tool_requires("cmake/[>=3.20 <4]")

    def validate(self):
        check_min_cppstd(self, 17)

    def source(self):
        get(self, **self.conan_data["sources"][self.version], strip_root=True)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.cache_variables["TARGET_UNIT_TESTS"] = False
        tc.cache_variables["TARGET_HELLO_WORLD"] = False
        tc.cache_variables["TARGET_PERFORMANCE_TEST"] = False
        tc.cache_variables["TARGET_SAMPLES"] = False
        tc.cache_variables["TARGET_VIEWER"] = False
        tc.cache_variables["CROSS_PLATFORM_DETERMINISTIC"] = False
        tc.cache_variables["CPP_RTTI_ENABLED"] = True
        tc.cache_variables["INTERPROCEDURAL_OPTIMIZATION"] = False
        tc.cache_variables["GENERATE_DEBUG_SYMBOLS"] = self.settings.build_type in ["Debug", "RelWithDebInfo"]
        tc.cache_variables["ENABLE_ALL_WARNINGS"] = False
        tc.cache_variables["OVERRIDE_CXX_FLAGS"] = False
        tc.cache_variables["DEBUG_RENDERER_IN_DISTRIBUTION"] = self.options.debug_renderer
        tc.cache_variables["DEBUG_RENDERER_IN_DEBUG_AND_RELEASE"] = self.options.debug_renderer
        tc.cache_variables["PROFILER_IN_DISTRIBUTION"] = self.options.profiler
        tc.cache_variables["PROFILER_IN_DEBUG_AND_RELEASE"] = self.options.profiler
        if is_msvc(self):
            tc.cache_variables["USE_STATIC_MSVC_RUNTIME_LIBRARY"] = is_msvc_static_runtime(self)
        tc.generate()

    def _patch_sources(self):
        cmakelists = os.path.join(self.source_folder, "Build", "CMakeLists.txt")
        replace_in_file(self, cmakelists, "if (CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR) # Only do this when we're at the top level", "if(0) #")

        jolt = os.path.join(self.source_folder, "Jolt", "Jolt.cmake")
        replace_in_file(self, jolt,
            "	target_compile_definitions(Jolt PUBLIC \"$<$<CONFIG:Debug,Release>:JPH_FLOATING_POINT_EXCEPTIONS_ENABLED>\")",
            "	target_compile_definitions(Jolt PUBLIC \"$<$<CONFIG:Debug,RelWithDebInfo,Release>:JPH_FLOATING_POINT_EXCEPTIONS_ENABLED>\")")

    def build(self):
        self._patch_sources()
        cmake = CMake(self)
        cmake.configure(build_script_folder=os.path.join(self.source_folder, "Build"))
        cmake.build()

    def package(self):
        copy(self, "LICENSE", src=self.source_folder, dst=os.path.join(self.package_folder, "licenses"))
        cmake = CMake(self)
        cmake.install()
        rmdir(self, os.path.join(self.package_folder, "lib", "cmake"))
        rm(self, "*.cmake", os.path.join(self.package_folder, "include", "Jolt"))

    def package_info(self):
        self.cpp_info.libs = ["Jolt"]
        self.cpp_info.set_property("cmake_file_name", "Jolt")
        self.cpp_info.set_property("cmake_target_name", "Jolt::Jolt")
        # INFO: The CMake option ENABLE_OBJECT_STREAM is enabled by default and defines JPH_OBJECT_STREAM as public
        # https://github.com/jrouwe/JoltPhysics/blob/v5.2.0/Build/CMakeLists.txt#L95C8-L95C28
        self.cpp_info.defines = ["JPH_OBJECT_STREAM"]
        # INFO: Public defines exposed in include/Jolt/Jolt.cmake
        # https://github.com/jrouwe/JoltPhysics/blob/v5.2.0/Build/CMakeLists.txt#L51
        if self.settings.arch in ["x86_64", "x86"]:
            self.cpp_info.defines.extend(["JPH_USE_AVX2", "JPH_USE_AVX", "JPH_USE_SSE4_1",
                                          "JPH_USE_SSE4_2", "JPH_USE_LZCNT", "JPH_USE_TZCNT",
                                          "JPH_USE_F16C", "JPH_USE_FMADD"])

        if is_msvc(self):
            self.cpp_info.defines.append("JPH_FLOATING_POINT_EXCEPTIONS_ENABLED")

        if self.options.debug_renderer:
            self.cpp_info.defines.append("JPH_DEBUG_RENDERER")

        if self.options.profiler:
            self.cpp_info.defines.append("JPH_PROFILE_ENABLED")

        if self.settings.os in ["Linux", "FreeBSD"]:
            self.cpp_info.system_libs.extend(["m", "pthread"])
