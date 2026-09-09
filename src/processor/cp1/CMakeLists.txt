add_library(cp1)
set_target_properties(cp1 PROPERTIES CXX_MODULE_STD ON)
target_sources(cp1 PUBLIC 
    FILE_SET modules TYPE CXX_MODULES 
    FILES
        module.cppm
        fragments/cp1.cppm
        fragments/instructionExecutor.cppm
        fragments/registers.cppm
)
target_compile_options(cp1 PUBLIC ${CXX_COMPILE_OPTIONS})
target_link_options(cp1 PUBLIC ${CXX_LINK_OPTIONS})
target_include_directories(cp1 PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(cp1 PUBLIC isa memory util)