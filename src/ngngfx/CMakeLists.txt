add_library(ngngfx)

target_sources(ngngfx
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include/ngngfx_aircraft_camera.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/include/ngngfx_camera.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/include/ngngfx_fixed_point_camera.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/include/ngngfx_gbuffer.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/include/ngngfx_orthographic_projection.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/include/ngngfx_projection.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/include/ngngfx_perspective_projection.hpp
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/ngngfx_aircraft_camera.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/ngngfx_camera.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/ngngfx_fixed_point_camera.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/ngngfx_gbuffer.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/ngngfx_orthographic_projection.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/ngngfx_projection.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/ngngfx_perspective_projection.cpp
)

target_include_directories(ngngfx
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(ngngfx
    PUBLIC
        glm_impl
        vkrndr
    PRIVATE
        $<BUILD_INTERFACE:project-options>
)

install(TARGETS ngngfx
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

add_library(niku::ngngfx ALIAS ngngfx)
