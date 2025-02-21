add_library(ngnscr)

target_sources(ngnscr
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include/ngnscr_types.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/include/ngnscr_scripting_engine.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/include/ngnscr_script_compiler.hpp
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/ngnscr_types.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/ngnscr_scripting_engine.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/ngnscr_script_compiler.cpp
)

target_include_directories(ngnscr
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(ngnscr
    PUBLIC
        cppext
    PUBLIC
        Angelscript::angelscript
        Boost::headers
    PRIVATE
        Angelscript::scriptbuilder
        Angelscript::scriptstdstring
        spdlog::spdlog
    PRIVATE
        $<BUILD_INTERFACE:project-options>
)

install(TARGETS ngnscr
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

add_library(niku::ngnscr ALIAS ngnscr)
