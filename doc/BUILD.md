# Building niku

## Initial setup
Override packages from public Conan2 package index with updated ones from [jan-kelemen/conan-recipes](https://github.com/jan-kelemen/conan-recipes):
```
git clone git@github.com:jan-kelemen/conan-recipes.git
conan export conan-recipes/recipes/fastgltf/all --version 0.8.0
conan export conan-recipes/recipes/freetype/meson --version 2.13.3
conan export conan-recipes/recipes/imgui/all --version 1.91.6-docking
conan export conan-recipes/recipes/joltphysics/all --version 5.2.0
conan export conan-recipes/recipes/meshoptimizer/all --version 0.22
conan export conan-recipes/recipes/sdl/all --version 2.30.11
conan export conan-recipes/recipes/simdjson/all --version 3.11.5
conan export conan-recipes/recipes/spdlog/all --version 1.15.x
conan export conan-recipes/recipes/spirv-headers/all --version 1.4.304.0
conan export conan-recipes/recipes/spirv-cross/all --version 1.4.304.0
conan export conan-recipes/recipes/spirv-tools/all --version 1.4.304.0
conan export conan-recipes/recipes/glslang/all --version 1.4.304.0
conan export conan-recipes/recipes/volk/all --version 1.4.304.0
conan export conan-recipes/recipes/vulkan-headers/all --version 1.4.304.0
conan export conan-recipes/recipes/vulkan-memory-allocator/all --version 3.2.0
```

## Installing dependencies
On Linux install Conan2 packages by executing:
```
conan install . --profile=conan/clang-19-libstdcxx-amd64-linux --profile=conan/dependencies --profile=conan/opt/linux-native --build=missing --settings build_type=Release
```

Or on Windows:
```
conan install . --profile=conan/msvc-2022-amd64-windows --profile=conan/dependencies --build=missing -s build_type=Release 
```

See folder [conan](../conan) for available profiles. Folder [ci/conan](../conan) contains preconfigured profile combinations for CI builds.

## Configure CMake build
Configure the build by using the required preset for the build type. Visual Studio builds use a `default` preset instead of the build type used when installing dependencies.
```
cmake --preset release
```

### Configuration options
* `NIKU_BUILD_DEMOS` - build engine demo examples, ON by default
* `NIKU_VKRNDR_ENABLE_DEBUG_UTILS` - debug utilities for Vulkan, ON by default in `Debug` and `RelWitDebInfo` configurations

## Compile
On Linux compile the code by executing:
```
cmake --build --preset release
```

Or on Windows:
```
cmake --build --preset multi-debug --config Debug
```