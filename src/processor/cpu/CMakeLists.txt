add_library(cpu)
set_target_properties(cpu PROPERTIES CXX_MODULE_STD ON)
target_sources(cpu PUBLIC 
    FILE_SET modules TYPE CXX_MODULES 
    FILES
        module.cppm
        fragments/cpu.cppm
)
target_sources(cpu PRIVATE
    fragments/cpuExecuteInstruction.cppm)
target_compile_options(cpu PUBLIC ${CXX_COMPILE_OPTIONS})
target_include_directories(cpu PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(cpu PUBLIC isa cp0 util rom memory)