add_library(ngntxt)

target_sources(ngntxt
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include/ngntxt_font_face.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/include/ngntxt_font_bitmap.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/include/ngntxt_shaping.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/include/ngntxt_syntax.hpp
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/ngntxt_font_face.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/ngntxt_font_bitmap.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/ngntxt_shaping.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/ngntxt_syntax.cpp
)

target_include_directories(ngntxt
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(ngntxt
    PUBLIC
        cppext
        glm_impl
        vkrndr
    PUBLIC
        Boost::headers
        Freetype::Freetype
        harfbuzz::harfbuzz
        tree-sitter::tree-sitter
    PRIVATE
        spdlog::spdlog
    PRIVATE
        $<BUILD_INTERFACE:project-options>
)

install(TARGETS ngntxt
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

add_library(niku::ngntxt ALIAS ngntxt)
