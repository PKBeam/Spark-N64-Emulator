add_library(memory)
target_sources(memory PUBLIC 
    FILE_SET modules TYPE CXX_MODULES 
    FILES
        module.cppm
        fragments/memory.cppm
        fragments/types.cppm
)
set_target_properties(memory PROPERTIES CXX_MODULE_STD ON)
target_compile_options(memory PUBLIC ${CXX_COMPILE_OPTIONS})
target_include_directories(memory PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(memory PUBLIC util rom interfaces)

