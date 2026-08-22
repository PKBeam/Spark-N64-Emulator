add_library(emulator)
set_target_properties(emulator PROPERTIES CXX_MODULE_STD ON)
target_sources(emulator PUBLIC 
    FILE_SET modules TYPE CXX_MODULES 
    FILES
        module.cppm
        fragments/emulator.cppm
)
target_compile_options(emulator PUBLIC ${CXX_COMPILE_OPTIONS})
target_include_directories(emulator PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(emulator PUBLIC util cpu rsp isa memory rom interfaces)

