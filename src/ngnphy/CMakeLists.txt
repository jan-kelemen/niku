add_library(ngnphy)

target_sources(ngnphy
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include/ngnphy_jolt_adapter.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/include/ngnphy_jolt_geometry.hpp
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/ngnphy_jolt_adapter.cpp
)

target_include_directories(ngnphy
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(ngnphy
    PUBLIC
        glm_impl
    PUBLIC
        Jolt::Jolt
    PRIVATE
        $<BUILD_INTERFACE:project-options>
)

install(TARGETS ngnphy
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

add_library(niku::ngnphy ALIAS ngnphy)
