add_library(interfaces)
set_target_properties(interfaces PROPERTIES CXX_MODULE_STD ON)
target_sources(interfaces PUBLIC 
    FILE_SET modules TYPE CXX_MODULES 
    FILES
        module.cppm
        fragments/mmioRegisters.cppm
        fragments/mipsInterface.cppm
        fragments/mipsInterfaceTypes.cppm
        fragments/rdramInterface.cppm
        fragments/rdramInterfaceTypes.cppm
        fragments/rdramRegisters.cppm
        fragments/rdramRegistersTypes.cppm
)
target_compile_options(interfaces PUBLIC ${CXX_COMPILE_OPTIONS})
target_include_directories(interfaces PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(interfaces PUBLIC util)

