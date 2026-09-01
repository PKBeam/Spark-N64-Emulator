add_library(interface_types)
set_target_properties(interface_types PROPERTIES CXX_MODULE_STD ON)
target_sources(interface_types PUBLIC 
    FILE_SET modules TYPE CXX_MODULES 
    FILES
        module.cppm
        fragments/audioInterfaceTypes.cppm
        fragments/mipsInterfaceTypes.cppm
        fragments/rdramInterfaceTypes.cppm
        fragments/rdramRegistersTypes.cppm
        fragments/rspRegistersTypes.cppm
        fragments/rdpRegistersTypes.cppm
        fragments/peripheralInterfaceTypes.cppm
        fragments/serialInterfaceTypes.cppm
        fragments/videoInterfaceTypes.cppm
)
target_compile_options(interface_types PUBLIC ${CXX_COMPILE_OPTIONS})
target_include_directories(interface_types PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(interface_types PUBLIC util)

