# niku - Development guide 

## Building 

### Initial setup
Override packages from public Conan2 package index with updated ones from [jan-kelemen/conan-recipes](https://github.com/jan-kelemen/conan-recipes):
```
git clone git@github.com:jan-kelemen/conan-recipes.git
conan export conan-recipes/recipes/angelscript/all --version 2.38.0
conan export conan-recipes/recipes/boost/all --version 1.89.0
conan export conan-recipes/recipes/catch2/3.x.x --version 3.10.0
conan export conan-recipes/recipes/entt/3.x.x --version 3.15.0
conan export conan-recipes/recipes/fastgltf/all --version 0.9.0
conan export conan-recipes/recipes/freetype/meson --version 2.14.1
conan export conan-recipes/recipes/glslang/all --version 1.4.321.0
conan export conan-recipes/recipes/harfbuzz/all --version 11.5.0
conan export conan-recipes/recipes/imgui/all --version 1.92.2b-docking
conan export conan-recipes/recipes/joltphysics/all --version 5.3.0
conan export conan-recipes/recipes/mikktspace/all --version cci.20200325
conan export conan-recipes/recipes/meshoptimizer/all --version 0.25
conan export conan-recipes/recipes/recastnavigation/all --version 1.6.0
conan export conan-recipes/recipes/sdl/3.x --version 3.2.22
conan export conan-recipes/recipes/simdjson/all --version 4.0.2
conan export conan-recipes/recipes/spdlog/all --version 1.15.3
conan export conan-recipes/recipes/spirv-headers/all --version 1.4.321.0
conan export conan-recipes/recipes/spirv-cross/all --version 1.4.321.0
conan export conan-recipes/recipes/spirv-tools/all --version 1.4.321.0
conan export conan-recipes/recipes/tree-sitter/all --version 0.25.9
conan export conan-recipes/recipes/tree-sitter-glsl/all --version 0.2.0
conan export conan-recipes/recipes/volk/all --version 1.4.321.0
conan export conan-recipes/recipes/vulkan-headers/all --version 1.4.321.0
conan export conan-recipes/recipes/vulkan-memory-allocator/all --version 3.3.0
conan export conan-recipes/recipes/vulkan-utility-libraries/all --version 1.4.321.0
```

Install Conan2 dependencies. See folder [conan](../conan) for available profiles. 
Folder [ci/conan](../ci/conan) contains preconfigured profile combinations for CI builds.

### Windows
```
conan install . --profile=conan/msvc-2022-amd64-windows --profile=conan/dependencies --build=missing -s build_type=Release 
cmake --preset release
cmake --build --preset multi-release --config Release
```

### Linux
```
conan install . --profile=conan/clang-20-libstdcxx-amd64-linux --profile=conan/dependencies --profile=conan/opt/linux-native --build=missing --settings build_type=Release
cmake --preset release
cmake --build --preset release
```

### CMake configuration options
* `NIKU_BUILD_DEMOS` - build engine demo examples, ON by default
* `NIKU_VKRNDR_ENABLE_DEBUG_UTILS` - debug utilities for Vulkan, ON by default in `Debug` and `RelWitDebInfo` configurations

## Working with sanitized builds
There are known leaks in X11 third party libraries [SDL/10322](https://github.com/libsdl-org/SDL/issues/10322).

If you need to work with a leak sanitizer enabled build use the suppressions file from the root of the repository:
```
LSAN_OPTIONS=suppressions=suppression.lsan ./gltfviewer
```
