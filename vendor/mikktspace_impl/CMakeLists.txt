add_library(mikktspace_impl)

target_sources(mikktspace_impl
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include/mikktspace.h
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/mikktspace.c
)

target_include_directories(mikktspace_impl
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

install(TARGETS mikktspace_impl
    EXPORT ${PROJECT_NAME}-config
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        COMPONENT ${PROJECT_NAME}_runtime
        NAMELINK_COMPONENT ${PROJECT_NAME}_dev
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        COMPONENT ${PROJECT_NAME}_dev
)

add_library(niku::mikktspace_impl ALIAS mikktspace_impl)

set_source_files_properties(
    ${CMAKE_CURRENT_SOURCE_DIR}/src/mikktspace.c 
    PROPERTIES
        SKIP_LINTING ON
)

set_target_properties(mikktspace_impl PROPERTIES FOLDER "vendor")

