add_library(as_scriptbuilder)

target_sources(as_scriptbuilder
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include/scriptbuilder/scriptbuilder.h
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/scriptbuilder.cpp
)

target_include_directories(as_scriptbuilder
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include/scriptbuilder
)

target_link_libraries(as_scriptbuilder
    PUBLIC
        Angelscript::angelscript
)

install(TARGETS as_scriptbuilder
    EXPORT ${PROJECT_NAME}-config
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        COMPONENT ${PROJECT_NAME}_runtime
        NAMELINK_COMPONENT ${PROJECT_NAME}_dev
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        COMPONENT ${PROJECT_NAME}_dev
)

add_library(as_scriptarray)

target_sources(as_scriptarray
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include/scriptarray/scriptarray.h
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/scriptarray.cpp
)

target_include_directories(as_scriptarray
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include/scriptarray
)

target_link_libraries(as_scriptarray
    PUBLIC
        Angelscript::angelscript
)

install(TARGETS as_scriptarray
    EXPORT ${PROJECT_NAME}-config
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        COMPONENT ${PROJECT_NAME}_runtime
        NAMELINK_COMPONENT ${PROJECT_NAME}_dev
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        COMPONENT ${PROJECT_NAME}_dev
)

add_library(as_scriptstdstring)

target_sources(as_scriptstdstring
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include/scriptstdstring/scriptstdstring.h
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/scriptstdstring.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/scriptstdstring_utils.cpp
)

target_include_directories(as_scriptstdstring
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include/scriptarray
        ${CMAKE_CURRENT_SOURCE_DIR}/include/scriptstdstring
)

target_link_libraries(as_scriptstdstring
    PUBLIC
        Angelscript::angelscript
    PRIVATE
        as_scriptarray
)

install(TARGETS as_scriptstdstring
    EXPORT ${PROJECT_NAME}-config
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        COMPONENT ${PROJECT_NAME}_runtime
        NAMELINK_COMPONENT ${PROJECT_NAME}_dev
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        COMPONENT ${PROJECT_NAME}_dev
)

set_source_files_properties(
    ${CMAKE_CURRENT_SOURCE_DIR}/src/scriptarray.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/scriptbuilder.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/scriptstdstring.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/scriptstdstring_utils.cpp
    PROPERTIES
        SKIP_LINTING ON
)

add_library(Angelscript::scriptarray ALIAS as_scriptarray)
add_library(Angelscript::scriptbuilder ALIAS as_scriptbuilder)
add_library(Angelscript::scriptstdstring ALIAS as_scriptstdstring)

add_library(niku::as_scriptarray ALIAS as_scriptarray)
add_library(niku::as_scriptbuilder ALIAS as_scriptbuilder)
add_library(niku::as_scriptstdstring ALIAS as_scriptstdstring)

set_target_properties(as_scriptarray PROPERTIES FOLDER "vendor")
set_target_properties(as_scriptbuilder PROPERTIES FOLDER "vendor")
set_target_properties(as_scriptstdstring PROPERTIES FOLDER "vendor")

