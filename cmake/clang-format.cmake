find_program(CLANG_FORMAT_EXE NAMES clang-format-19 clang-format REQUIRED)
message(STATUS "clang-format found: ${CLANG_FORMAT_EXE}")

file(GLOB_RECURSE ALL_SOURCE_FILES 
    src/*.h src/*.c 
    src/*.hpp src/*.cpp 
    demo/*.h demo/*.c 
    demo/*.hpp demo/*.cpp
)

add_custom_target(clang-format 
    COMMAND 
        ${CLANG_FORMAT_EXE} -style=file -i ${ALL_SOURCE_FILES}
)

if (NIKU_ENABLE_GLSL_FORMAT)
    file(GLOB_RECURSE ALL_SHADER_FILES
        demo/*.comp demo/*.frag demo/*.glsl demo/*.vert
    )

    add_custom_target(glsl-format 
        COMMAND 
            ${CLANG_FORMAT_EXE} -style=file --qualifier-alignment=Left -i ${ALL_SHADER_FILES}
    )
endif()