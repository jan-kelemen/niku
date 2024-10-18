# niku [![GitHub Build status](https://github.com/jan-kelemen/niku/actions/workflows/ci.yml/badge.svg?branch=master)](https://github.com/jan-kelemen/niku/actions/workflows/ci.yml)

Cross platform game engine using Vulkan API for rendering. Implemented in C++23 with support for Windows and Linux.

## Building
Necessary build tools are:
* CMake 3.27 or higher
* Conan 2.4 or higher
  * See [installation instructions](https://docs.conan.io/2/installation.html)
* One of supported compilers:
  * Clang-18 (libstdc++ and libc++)
  * GCC-14
  * Visual Studio 2022 (MSVC v194)

```
conan install . --profile=conan/clang-18-libstdcxx-amd64-linux --profile=conan/dependencies --build=missing --settings build_type=Release
cmake --preset release
cmake --build --preset=release
```

### Configuration options
* `NIKU_BUILD_DEMOS` - build engine demo examples, ON by default

Note: Override packages from Conan Center with updated ones from [jan-kelemen/conan-recipes](https://github.com/jan-kelemen/conan-recipes), this is mostly due to hardening or sanitizer options being incompatible with packages on Conan Center.
```
git clone git@github.com:jan-kelemen/conan-recipes.git
conan export conan-recipes/recipes/fastgltf/all --version 0.8.0
conan export conan-recipes/recipes/freetype/meson --version 2.13.3
conan export conan-recipes/recipes/sdl/all --version 2.30.8
conan export conan-recipes/recipes/simdjson/all --version 3.10.1
conan export conan-recipes/recipes/spdlog/all --version 1.14.1
conan export conan-recipes/recipes/vulkan-memory-allocator/all --version 3.1.0
```

And then execute the build commands.
