add_library(glm_impl INTERFACE)

target_compile_definitions(glm_impl
    INTERFACE
        GLM_FORCE_RADIANS
        GLM_ENABLE_EXPERIMENTAL
)

target_link_libraries(glm_impl
    INTERFACE
        glm::glm
)

set_target_properties(glm_impl PROPERTIES FOLDER "vendor")
