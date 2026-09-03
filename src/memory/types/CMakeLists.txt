add_library(memory_types)
target_sources(memory_types PUBLIC 
    FILE_SET modules TYPE CXX_MODULES 
    FILES
        module.cppm
        fragments/memory.cppm
        fragments/segments.cppm
)
set_target_properties(memory_types PROPERTIES CXX_MODULE_STD ON)
target_compile_options(memory_types PUBLIC ${CXX_COMPILE_OPTIONS})
target_include_directories(memory_types PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(memory_types PUBLIC util)

