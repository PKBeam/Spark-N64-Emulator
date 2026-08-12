add_library(isa)
set_target_properties(isa PROPERTIES CXX_MODULE_STD ON)
target_sources(isa PUBLIC 
    FILE_SET modules TYPE CXX_MODULES 
    FILES
        module.cppm
        fragments/instruction.cppm 
        fragments/instructionData.cppm
        fragments/opcodes.cppm
        fragments/operands.cppm
        fragments/registers.cppm
)
target_compile_options(isa PUBLIC ${CXX_COMPILE_OPTIONS})
target_include_directories(isa PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(isa PUBLIC util)

