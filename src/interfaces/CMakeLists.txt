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
        fragments/peripheralInterface.cppm
        fragments/serialInterface.cppm
        fragments/videoInterface.cppm

        fragments/types/audioInterfaceTypes.cppm
        fragments/types/mipsInterfaceTypes.cppm
        fragments/types/rdramInterfaceTypes.cppm
        fragments/types/rdramRegistersTypes.cppm
        fragments/types/rspRegistersTypes.cppm
        fragments/types/peripheralInterfaceTypes.cppm
        fragments/types/serialInterfaceTypes.cppm
        fragments/types/videoInterfaceTypes.cppm
)
target_compile_options(interfaces PUBLIC ${CXX_COMPILE_OPTIONS})
target_include_directories(interfaces PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(interfaces PUBLIC cp0 util isa rom)

