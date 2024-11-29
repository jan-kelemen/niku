# niku [![GitHub CI](https://github.com/jan-kelemen/niku/actions/workflows/ci.yml/badge.svg?branch=master)](https://github.com/jan-kelemen/niku/actions/workflows/ci.yml)

Cross platform game engine implemented in C++23 with support for Windows and Linux.

## Features
* Vulkan 1.3 rendering backend
* Support for runtime shader compilation and reflection with glslang and SPIRV-Cross
* Support for loading glTF files

## Demo applications
* [gltfviewer](https://github.com/jan-kelemen/niku/tree/master/demo/gltfviewer) - 3D renderer for glTF files implementing a Physically Based Rendering workflow

## Building
Necessary build tools are:
* CMake 3.27 or higher
* Conan 2.8 or higher
  * See [installation instructions](https://docs.conan.io/2/installation.html)
* One of supported compilers:
  * Clang-19 (libstdc++ or libc++)
  * GCC-14
  * Visual Studio 2022 (MSVC v194)

Override packages from public Conan2 package index with updated ones from [jan-kelemen/conan-recipes](https://github.com/jan-kelemen/conan-recipes):
```
git clone git@github.com:jan-kelemen/conan-recipes.git
conan export conan-recipes/recipes/fastgltf/all --version 0.8.0
conan export conan-recipes/recipes/freetype/meson --version 2.13.3
conan export conan-recipes/recipes/joltphysics/all --version 5.2.0
conan export conan-recipes/recipes/meshoptimizer/all --version 0.22
conan export conan-recipes/recipes/sdl/all --version 2.30.9
conan export conan-recipes/recipes/spirv-cross/all --version 1.3.296.0
conan export conan-recipes/recipes/spirv-tools/all --version 1.3.296.0
conan export conan-recipes/recipes/glslang/all --version 1.3.296.0
conan export conan-recipes/recipes/vulkan-memory-allocator/all --version 3.1.0
```

Compile:
```
conan install . --profile=conan/clang-19-libstdcxx-amd64-linux --profile=conan/dependencies --build=missing --settings build_type=Release
cmake --preset release
cmake --build --preset=release
```

### CMake Configuration Options
* `NIKU_BUILD_DEMOS` - build engine demo examples, ON by default

## License
Licensed under [BSD-3-Clause](https://github.com/jan-kelemen/niku/blob/master/LICENSE). See [COPYRIGHT.md](https://github.com/jan-kelemen/niku/blob/master/COPYRIGHT.md) for additional licensing info
