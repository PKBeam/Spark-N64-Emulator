add_library(rdp_control)
set_target_properties(rdp_control PROPERTIES CXX_MODULE_STD ON)
target_sources(rdp_control PUBLIC 
    FILE_SET modules TYPE CXX_MODULES 
    FILES
        module.cppm
        fragments/rdpControl.cppm
)
target_compile_options(rdp_control PUBLIC ${CXX_COMPILE_OPTIONS})
target_include_directories(rdp_control PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(rdp_control PUBLIC interface_types isa memory_types util)