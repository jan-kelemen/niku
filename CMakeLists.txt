cmake_minimum_required(VERSION 3.27)

cmake_policy(SET CMP0083 NEW)

project(niku VERSION 0.1 LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(NIKU_ENABLE_CLANG_FORMAT "Enable clang-format in build" OFF)
option(NIKU_ENABLE_CLANG_TIDY "Enable clang-tidy in build" OFF)
option(NIKU_ENABLE_COMPILER_STATIC_ANALYSIS "Enable static analysis provided by compiler in build" OFF)
option(NIKU_ENABLE_CPPCHECK "Enable cppcheck in build" OFF)
option(NIKU_ENABLE_IWYU "Enable include-what-you-use in build" OFF)

option(NIKU_BUILD_DEMOS "Building demo examples" ON)

find_package(fmt REQUIRED)
find_package(imgui REQUIRED)
find_package(glm REQUIRED)
find_package(SDL2 REQUIRED)
find_package(spdlog REQUIRED)
find_package(stb REQUIRED)
find_package(VulkanHeaders REQUIRED)
find_package(VulkanLoader REQUIRED)
find_package(vulkan-memory-allocator REQUIRED)

if (NIKU_BUILD_DEMOS)
    find_package(fastgltf REQUIRED)
    find_package(simdjson REQUIRED)
endif()

if (CMAKE_POSITION_INDEPENDENT_CODE)
    include(CheckPIESupported)
    check_pie_supported(OUTPUT_VARIABLE pie_supported LANGUAGES CXX)
    if(NOT CMAKE_CXX_LINK_PIE_SUPPORTED)
        message(WARNING "PIE is not supported at link time: ${pie_supported}.\n PIE link options will not be passed to linker.")
    else()
        message(STATUS "PIE flags enabled for executable targets.")
    endif()
endif()

if (CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    include(CTest)
    if (BUILD_TESTING)
        find_package(Catch2)
        set(NIKU_BUILD_TESTS ON)
    endif()

    include(${PROJECT_SOURCE_DIR}/cmake/clang-tidy.cmake)
    include(${PROJECT_SOURCE_DIR}/cmake/cppcheck.cmake)
    include(${PROJECT_SOURCE_DIR}/cmake/iwyu.cmake)

    include(${PROJECT_SOURCE_DIR}/cmake/project-options.cmake)
endif()

include(${PROJECT_SOURCE_DIR}/cmake/glslc.cmake)

add_subdirectory(src)
add_subdirectory(vendor)

if (NIKU_BUILD_DEMOS)
    add_subdirectory(demo)
endif()

