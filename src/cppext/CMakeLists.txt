add_library(cppext INTERFACE)

target_sources(cppext
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include/cppext_attribute.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/include/cppext_overloaded.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/include/cppext_numeric.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/include/cppext_pragma_warning.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/include/cppext_cycled_buffer.hpp
)

target_include_directories(cppext
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(cppext
    INTERFACE
        project-options
)

if (NIKU_BUILD_TESTS)
    add_executable(cppext_test)

    target_sources(cppext_test
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/test/cppext_cycled_buffer.t.cpp
    )

    target_link_libraries(cppext_test
        PUBLIC
            cppext
        PRIVATE
            Catch2::Catch2WithMain
            project-options
    )

    if (NOT CMAKE_CROSSCOMPILING)
        include(Catch)
        catch_discover_tests(cppext_test)
    endif()
endif()
