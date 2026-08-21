add_library(cp0)
set_target_properties(cp0 PROPERTIES CXX_MODULE_STD ON)
target_sources(cp0 PUBLIC 
    FILE_SET modules TYPE CXX_MODULES 
    FILES
        module.cppm
        fragments/cp0.cppm
)
target_compile_options(cp0 PUBLIC ${CXX_COMPILE_OPTIONS})
target_include_directories(cp0 PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(cp0 PUBLIC isa util)