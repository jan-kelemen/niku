add_library(thread_pool_impl INTERFACE)

target_compile_definitions(thread_pool_impl
    INTERFACE
        BS_THREAD_POOL_NATIVE_EXTENSIONS
)

target_link_libraries(thread_pool_impl
    INTERFACE
        bshoshany-thread-pool::bshoshany-thread-pool
)
install(TARGETS thread_pool_impl
    EXPORT ${PROJECT_NAME}-config
)

add_library(niku::thread_pool_impl ALIAS thread_pool_impl)

set_target_properties(thread_pool_impl PROPERTIES FOLDER "vendor")
