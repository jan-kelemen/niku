if (NOT GLSLC_EXE)
    find_program(GLSLC_EXE NAMES glslc REQUIRED)
    message(STATUS "glslc found: ${GLSLC_EXE}")
endif()

function(compile_shader)
    set(options)
    set(oneValueArgs SHADER SPIRV)
    set(multiValueArgs)
    cmake_parse_arguments(
        GLSLC_SHADER "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN}
    )

    add_custom_command(
        OUTPUT ${GLSLC_SHADER_SPIRV}
        COMMAND ${GLSLC_EXE}
            $<$<OR:$<CONFIG:RelWithDebInfo>,$<CONFIG:Release>>:-O> # Optimize in RelWithDebInfo and Release
            $<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:-g> # Add debug information in Debug or RelWithDebInfo
            ${GLSLC_SHADER_SHADER} -o ${GLSLC_SHADER_SPIRV}
        DEPENDS ${GLSLC_SHADER_SHADER}
    )
endfunction()
