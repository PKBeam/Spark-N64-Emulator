add_library(rsp)
set_target_properties(rsp PROPERTIES CXX_MODULE_STD ON)
target_sources(rsp PUBLIC 
    FILE_SET modules TYPE CXX_MODULES 
    FILES
        module.cppm
        fragments/rsp.cppm
        fragments/instructionExecutor.cppm
        fragments/registers.cppm
)
target_compile_options(rsp PUBLIC ${CXX_COMPILE_OPTIONS})
target_link_options(rsp PUBLIC ${CXX_LINK_OPTIONS})
target_include_directories(rsp PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(rsp PUBLIC cpu interfaces interface_types memory rsp_control util)

