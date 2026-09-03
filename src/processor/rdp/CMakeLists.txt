add_library(rdp)
set_target_properties(rdp PROPERTIES CXX_MODULE_STD ON)
target_sources(rdp PUBLIC 
    FILE_SET modules TYPE CXX_MODULES 
    FILES
        module.cppm
        fragments/rdp.cppm
        fragments/rdpCommands.cppm
)
target_compile_options(rdp PUBLIC ${CXX_COMPILE_OPTIONS})
target_include_directories(rdp PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(rdp PUBLIC interfaces interface_types isa rdp_control util)