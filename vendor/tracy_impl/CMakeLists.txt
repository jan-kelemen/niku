add_library(tracy_impl INTERFACE)

target_include_directories(tracy_impl
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

if (Tracy_FOUND)
    target_link_libraries(tracy_impl
        INTERFACE
            Tracy::TracyClient
    )
else()
    target_compile_definitions(tracy_impl
        INTERFACE
            TRACY_DISABLE)
endif()

install(TARGETS tracy_impl
    EXPORT ${PROJECT_NAME}-config
)

add_library(niku::tracy_impl ALIAS tracy_impl)

set_target_properties(tracy_impl PROPERTIES FOLDER "vendor")
