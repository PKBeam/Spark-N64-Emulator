add_library(rsp_control)
set_target_properties(rsp_control PROPERTIES CXX_MODULE_STD ON)
target_sources(rsp_control PUBLIC 
    FILE_SET modules TYPE CXX_MODULES 
    FILES
        module.cppm
        fragments/rspControl.cppm
)
target_compile_options(rsp_control PUBLIC ${CXX_COMPILE_OPTIONS})
target_include_directories(rsp_control PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(rsp_control PUBLIC util)

