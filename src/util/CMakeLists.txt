add_library(util)
set_target_properties(util PROPERTIES CXX_MODULE_STD ON)
target_sources(util PUBLIC 
    FILE_SET modules TYPE CXX_MODULES 
    FILES
        module.cppm
        fragments/bit.cppm 
        fragments/file.cppm
        fragments/log.cppm
        fragments/meta.cppm
        fragments/types.cppm
)
target_compile_options(util PRIVATE ${CXX_COMPILE_OPTIONS})
target_include_directories(util PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
