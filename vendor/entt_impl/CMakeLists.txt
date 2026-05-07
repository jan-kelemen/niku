add_library(entt_impl INTERFACE)

target_compile_definitions(entt_impl
    INTERFACE
        ENTT_USE_ATOMIC
)

target_link_libraries(entt_impl
    INTERFACE
        EnTT::EnTT
)

install(TARGETS entt_impl
    EXPORT ${PROJECT_NAME}-config
)

add_library(niku::entt_impl ALIAS entt_impl)

set_target_properties(entt_impl PROPERTIES FOLDER "vendor")
