add_library(rom)
set_target_properties(rom PROPERTIES CXX_MODULE_STD ON)
target_sources(rom PUBLIC 
    FILE_SET modules TYPE CXX_MODULES 
    FILES
        module.cppm
        fragments/file.cppm
        fragments/types.cppm
)
target_compile_options(rom PUBLIC ${CXX_COMPILE_OPTIONS})
target_include_directories(rom PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(rom PUBLIC util)

