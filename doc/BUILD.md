# Building niku

## Initial setup
Override packages from public Conan2 package index with updated ones from [jan-kelemen/conan-recipes](https://github.com/jan-kelemen/conan-recipes):
```
git clone git@github.com:jan-kelemen/conan-recipes.git
conan export conan-recipes/recipes/angelscript/all --version 2.37.0
conan export conan-recipes/recipes/boost/all --version 1.87.0
conan export conan-recipes/recipes/entt/3.x.x --version 3.15.0
conan export conan-recipes/recipes/fastgltf/all --version 0.8.0
conan export conan-recipes/recipes/freetype/meson --version 2.13.3
conan export conan-recipes/recipes/glslang/all --version 1.4.304.0
conan export conan-recipes/recipes/harfbuzz/all --version 11.2.0
conan export conan-recipes/recipes/imgui/all --version 1.91.9b-docking
conan export conan-recipes/recipes/joltphysics/all --version 5.3.0
conan export conan-recipes/recipes/mikktspace/all --version cci.20200325
conan export conan-recipes/recipes/meshoptimizer/all --version 0.23
conan export conan-recipes/recipes/recastnavigation/all --version 1.6.0
conan export conan-recipes/recipes/sdl/3.x --version 3.2.8
conan export conan-recipes/recipes/simdjson/all --version 3.12.2
conan export conan-recipes/recipes/spdlog/all --version 1.15.2
conan export conan-recipes/recipes/spirv-headers/all --version 1.4.304.0
conan export conan-recipes/recipes/spirv-cross/all --version 1.4.304.0
conan export conan-recipes/recipes/spirv-tools/all --version 1.4.304.0
conan export conan-recipes/recipes/volk/all --version 1.4.304.0
conan export conan-recipes/recipes/vulkan-headers/all --version 1.4.304.0
conan export conan-recipes/recipes/vulkan-memory-allocator/all --version 3.2.1
conan export conan-recipes/recipes/tree-sitter/all --version 0.25.3
conan export conan-recipes/recipes/tree-sitter-glsl/all --version 0.2.0
```

Install Conan2 dependencies. See folder [conan](../conan) for available profiles. 
Folder [ci/conan](../ci/conan) contains preconfigured profile combinations for CI builds.

## Windows
```
conan install . --profile=conan/msvc-2022-amd64-windows --profile=conan/dependencies --build=missing -s build_type=Release 
cmake --preset release
cmake --build --preset multi-release --config Release
```

## Linux
```
conan install . --profile=conan/clang-20-libstdcxx-amd64-linux --profile=conan/dependencies --profile=conan/opt/linux-native --build=missing --settings build_type=Release
cmake --preset release
cmake --build --preset release
```

## CMake configuration options
* `NIKU_BUILD_DEMOS` - build engine demo examples, ON by default
* `NIKU_VKRNDR_ENABLE_DEBUG_UTILS` - debug utilities for Vulkan, ON by default in `Debug` and `RelWitDebInfo` configurations

