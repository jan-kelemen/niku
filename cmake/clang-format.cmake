find_program(CLANG_FORMAT_EXE NAMES clang-format-18 clang-format REQUIRED)
message(STATUS "clang-format found: ${CLANG_FORMAT_EXE}")

file(GLOB_RECURSE ALL_SOURCE_FILES src/*.h src/*.c src/*.hpp src/*.cpp)

add_custom_target(clang-format 
    COMMAND 
        ${CLANG_FORMAT_EXE} -style=file -i ${ALL_SOURCE_FILES}
)

