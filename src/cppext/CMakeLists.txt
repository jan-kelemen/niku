add_library(cppext INTERFACE)

target_sources(cppext
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include/cppext_attribute.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/include/cppext_container.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/include/cppext_cycled_buffer.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/include/cppext_memory.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/include/cppext_numeric.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/include/cppext_overloaded.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/include/cppext_pragma_warning.hpp
        
)

target_include_directories(cppext 
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_link_libraries(cppext
    INTERFACE
        $<BUILD_INTERFACE:project-options>
)

install(TARGETS cppext
    EXPORT ${PROJECT_NAME}-config
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

add_library(niku::cppext ALIAS cppext)

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
            $<BUILD_INTERFACE:project-options>
    )

    if (NOT CMAKE_CROSSCOMPILING)
        include(Catch)
        catch_discover_tests(cppext_test)
    endif()
endif()
