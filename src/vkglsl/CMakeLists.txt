add_library(vkglsl)

target_sources(vkglsl
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include/vkglsl_guard.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/include/vkglsl_shader_set.hpp
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/vkglsl_guard.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/vkglsl_glslang_adapter.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/vkglsl_spirv_cross_adapter.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/vkglsl_spirv_cross_adapter.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/vkglsl_shader_set.cpp
)

target_include_directories(vkglsl
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(vkglsl
    PUBLIC
        vkrndr
    PRIVATE
        cppext
    PRIVATE
        glslang::glslang
        glslang::glslang-default-resource-limits
        glslang::SPIRV
        spdlog::spdlog
        spirv-cross::spirv-cross
    PRIVATE
        $<BUILD_INTERFACE:project-options>
)

install(TARGETS vkglsl
    EXPORT ${PROJECT_NAME}-config
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        COMPONENT ${PROJECT_NAME}_runtime
        NAMELINK_COMPONENT ${PROJECT_NAME}_dev
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        COMPONENT ${PROJECT_NAME}_dev
)

install(DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
add_library(niku::vkglsl ALIAS vkglsl)
