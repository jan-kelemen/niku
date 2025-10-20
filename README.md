# niku [![GitHub CI](https://github.com/jan-kelemen/niku/actions/workflows/ci.yml/badge.svg?branch=master)](https://github.com/jan-kelemen/niku/actions/workflows/ci.yml)

Cross platform game engine implemented in C++23 with support for Windows and Linux.

## Features
* Vulkan 1.3 rendering backend
* Support for runtime shader compilation and reflection with glslang and SPIRV-Cross
* Support for loading glTF files
* Support for 3D physics simulation with Jolt library
* Support for scripting with AngelScript language

## Demo applications
* [galileo](demo/galileo) - Deferred 3D renderer with physics and scripting
* [gltfviewer](demo/gltfviewer) - Forward 3D renderer for glTF files implementing a Physically Based Rendering workflow
* [reshed](demo/reshed) - Text editor with bitmap font rendering and GLSL syntax highlighting

## Building

Necessary build tools are:
* CMake 3.27 or higher
* Conan 2.20 or higher
  * See [installation instructions](https://docs.conan.io/2/installation.html)
* One of supported compilers:
  * Clang-21 (libstdc++ or libc++)
  * GCC-14
  * Visual Studio 2022 (MSVC v194)
* Ninja (if using Clang on Windows)

See [doc/DEVELOPMENT.md](doc/DEVELOPMENT.md) for details.

## License
Licensed under [BSD-3-Clause](LICENSE). See [COPYRIGHT.md](COPYRIGHT.md) for additional licensing info
