add_library(ngngfx)

target_sources(ngngfx
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include/ngngfx_camera.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/include/ngngfx_gbuffer.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/include/ngngfx_perspective_camera.hpp
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/ngngfx_camera.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/ngngfx_gbuffer.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/ngngfx_perspective_camera.cpp
)

target_include_directories(ngngfx
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(ngngfx
    PUBLIC
        glm_impl
        vkrndr
    PRIVATE
        project-options
)

