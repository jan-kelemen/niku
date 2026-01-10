import os

from conan import ConanFile
from conan.errors import ConanInvalidConfiguration
from conan.tools.build import check_min_cppstd
from conan.tools.cmake import CMake, CMakeDeps, CMakeToolchain, cmake_layout
from conan.tools.files import apply_conandata_patches, collect_libs, copy, export_conandata_patches, get, rmdir
from conan.tools.microsoft import is_msvc
from conan.tools.scm import Version

required_conan_version = ">=2.1"


class LibBasisUniversalConan(ConanFile):
    name = "libbasisu"
    description = "Basis Universal Supercompressed GPU Texture Codec"
    license = "Apache-2.0"
    url = "https://github.com/conan-io/conan-center-index"
    homepage = "https://github.com/BinomialLLC/basis_universal"
    topics = ("basis", "textures", "compression")

    package_type = "library"
    settings = "os", "arch", "compiler", "build_type"
    options = {
        "shared": [True, False],
        "fPIC": [True, False],
        "use_sse4": [True, False],
        "with_zstd": [True, False],
        "build_encoder": [True, False],
        "build_decoder": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
        "use_sse4": False,
        "with_zstd": True,
        "build_encoder": True,
        "build_decoder": True,
    }

    def export_sources(self):
        export_conandata_patches(self)

    def config_options(self):
        if self.settings.os == "Windows":
            del self.options.fPIC

    def configure(self):
        if self.options.shared:
            self.options.rm_safe("fPIC")

    def requirements(self):
        if self.options.with_zstd:
            self.requires("zstd/1.5.7")

    def layout(self):
        cmake_layout(self, src_folder="src")

    def validate(self):
        check_min_cppstd(self, 17)
        if self.options.build_encoder:
            raise ConanInvalidConfiguration("TODO-JK: Fix 3rd party code references in source to Conan packages")

    def source(self):
        get(self, **self.conan_data["sources"][self.version], strip_root=True)
        rmdir(self, os.path.join(self.source_folder, "webgl")) 
        rmdir(self, os.path.join(self.source_folder, "test_files"))
        apply_conandata_patches(self)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["SSE4"] = self.options.use_sse4
        tc.variables["ZSTD"] = self.options.with_zstd
        tc.variables["BASISU_BUILD_ENCODER"] = self.options.build_encoder
        tc.variables["BASISU_BUILD_DECODER"] = self.options.build_decoder
        tc.variables["OPENCL"] = False
        tc.variables["EXAMPLES"] = False
        tc.generate()
        tc = CMakeDeps(self)
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        copy(self, "LICENSE",
             dst=os.path.join(self.package_folder, "licenses"),
             src=self.source_folder)
        if self.options.build_decoder:
            copy(self, "*.h",
                dst=os.path.join(self.package_folder, "include", self.name, "transcoder"),
                src=os.path.join(self.source_folder, "transcoder"))
        if self.options.build_encoder:
            copy(self,"*.h",
                dst=os.path.join(self.package_folder, "include", self.name, "encoder"),
                src=os.path.join(self.source_folder, "encoder"))
        for pattern in ["*.a", "*.so*", "*.dylib*", "*.lib"]:
            copy(self, pattern,
                 dst=os.path.join(self.package_folder, "lib"),
                 src=self.build_folder, keep_path=False)
        copy(self, "*.dll",
             dst=os.path.join(self.package_folder, "bin"),
             src=self.build_folder, keep_path=False)

    def package_info(self):
        self.cpp_info.libs = collect_libs(self)
        self.cpp_info.includedirs = ["include", os.path.join("include", self.name)]
        if self.settings.os in ["Linux", "FreeBSD"]:
            self.cpp_info.system_libs = ["m", "pthread"]
