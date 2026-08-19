add_library(cpu)
set_target_properties(cpu PROPERTIES CXX_MODULE_STD ON)
target_sources(cpu PUBLIC 
    FILE_SET modules TYPE CXX_MODULES 
    FILES
        module.cppm
        fragments/vr4300.cppm
)
target_sources(cpu PRIVATE
    fragments/vr4300executeInstruction.cppm)
target_compile_options(cpu PUBLIC ${CXX_COMPILE_OPTIONS})
target_include_directories(cpu PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(cpu PUBLIC isa util rom memory)