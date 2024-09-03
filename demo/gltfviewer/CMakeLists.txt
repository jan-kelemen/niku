add_executable(gltfviewer)

target_sources(gltfviewer
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/application.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/model_selector.hpp
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/application.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/model_selector.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/gltfviewer.m.cpp
)

target_include_directories(gltfviewer
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(gltfviewer
    PRIVATE
        niku
        vkgltf
    PRIVATE
        fmt::fmt
        imgui::imgui
        simdjson::simdjson
        spdlog::spdlog
        Vulkan::Loader
    PRIVATE
        project-options
)

set_property(TARGET gltfviewer 
    PROPERTY 
        VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

set_target_properties(gltfviewer PROPERTIES FOLDER "demo")
