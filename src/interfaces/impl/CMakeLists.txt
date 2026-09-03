add_library(interfaces)
set_target_properties(interfaces PROPERTIES CXX_MODULE_STD ON)
target_sources(interfaces PUBLIC 
    FILE_SET modules TYPE CXX_MODULES 
    FILES
        module.cppm
        
        fragments/interface.cppm
        
        fragments/audioInterface.cppm
        fragments/mipsInterface.cppm
        fragments/rdramInterface.cppm
        fragments/rdramRegisters.cppm
        fragments/rspRegisters.cppm
        fragments/rdpRegisters.cppm
        fragments/peripheralInterface.cppm
        fragments/serialInterface.cppm
        fragments/videoInterface.cppm
)
target_compile_options(interfaces PUBLIC ${CXX_COMPILE_OPTIONS})
target_include_directories(interfaces PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(interfaces PUBLIC cp0 memory_types rdp_control rsp_control util isa rom)

