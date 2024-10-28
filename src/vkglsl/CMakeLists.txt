add_library(vkglsl)

target_sources(vkglsl
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include/vkglsl_shader_set.hpp
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/vkglsl_shader_set.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/vkglsl_glslang_adapter.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/vkglsl_glslang_adapter.hpp
)

target_include_directories(vkglsl
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(vkglsl
    PUBLIC
        volk::volk
    PRIVATE
        cppext
    PRIVATE
        glslang::glslang
        glslang::glslang-default-resource-limits
        glslang::SPIRV
        spdlog::spdlog
    PRIVATE
        project-options
)

