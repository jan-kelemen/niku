# niku - Development guide 

## Building 

### Initial setup
Add a Conan remote:
```
conan remote add niku-remote conan/index
```

Install Conan2 dependencies. See folder [conan](../conan) for available profiles. 
Folder [ci/conan](../ci/conan) contains preconfigured profile combinations for CI builds.

Recipes of Conan packages are managed in the [jan-kelemen/conan-recipes](https://github.com/jan-kelemen/conan-recipes) repository, `conan/index` is a git subtree.

Use the following command to update the recipes index:
```
git subtree --prefix conan/index pull --squash git@github.com:jan-kelemen/conan-recipes.git master
```

### Windows
```
conan install . --profile=conan/msvc-2022-amd64-windows --profile=conan/dependencies --build=missing -s build_type=Release -r niku-remote
cmake --preset release
cmake --build --preset multi-release --config Release
```

### Linux
```
conan install . --profile=conan/clang-20-libstdcxx-amd64-linux --profile=conan/dependencies --profile=conan/opt/linux-native --build=missing --settings build_type=Release  -r niku-remote
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
