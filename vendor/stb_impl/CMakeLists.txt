add_library(stb_impl)

target_sources(stb_impl
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/stb_image_impl.cpp
)

target_link_libraries(stb_impl
    PUBLIC
        stb::stb
)

set_source_files_properties(
    ${CMAKE_CURRENT_SOURCE_DIR}/src/stb_image_impl.cpp 
    PROPERTIES
        SKIP_LINTING ON
)

install(TARGETS stb_impl
    EXPORT ${PROJECT_NAME}-config
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        COMPONENT ${PROJECT_NAME}_runtime
        NAMELINK_COMPONENT ${PROJECT_NAME}_dev
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        COMPONENT ${PROJECT_NAME}_dev
)

add_library(niku::stb_impl ALIAS stb_impl)

set_target_properties(stb_impl PROPERTIES FOLDER "vendor")
