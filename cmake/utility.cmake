function(add_copy_if_different_target target_name)
    cmake_parse_arguments(PARSE_ARGV 1 CID_PREFIX "" "DESTINATION" "FILES")

    add_custom_target(${target_name}
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CID_PREFIX_FILES}
            ${CID_PREFIX_DESTINATION}
        DEPENDS
            ${CID_PREFIX_FILES}
    )

    target_sources(${target_name} PRIVATE ${CID_PREFIX_FILES})
endfunction()
