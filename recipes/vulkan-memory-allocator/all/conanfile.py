from conan import ConanFile
from conan.tools.build import check_min_cppstd
from conan.tools.files import copy, get
from conan.tools.layout import basic_layout
from conan.tools.scm import Version
import os

required_conan_version = ">2.0"


class VulkanMemoryAllocatorConan(ConanFile):
    name = "vulkan-memory-allocator"
    license = "MIT"
    homepage = "https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator"
    url = "https://github.com/conan-io/conan-center-index"
    description = "Easy to integrate Vulkan memory allocation library."
    topics = ("vulkan", "memory-allocator", "graphics")
    package_type = "header-library"
    settings = "os", "arch", "compiler", "build_type"

    def layout(self):
        basic_layout(self, src_folder="src")

    def requirements(self):
        self.requires("vulkan-headers/[^1.4]")

    def package_id(self):
        self.info.clear()

    def validate(self):
        if self.settings.compiler.get_safe("cppstd"):
            check_min_cppstd(self, 14)

    def source(self):
        get(self, **self.conan_data["sources"][self.version],
            destination=self.source_folder, strip_root=True)

    def package(self):
        copy(self, "LICENSE.txt", src=self.source_folder, dst=os.path.join(self.package_folder, "licenses"))
        include_dir = os.path.join(self.source_folder, "include")
        copy(self, "vk_mem_alloc.h", src=include_dir, dst=os.path.join(self.package_folder, "include"))

    def package_info(self):
        self.cpp_info.bindirs = []
        self.cpp_info.libdirs = []
