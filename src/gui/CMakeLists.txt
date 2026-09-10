add_library(gui)
set_target_properties(gui PROPERTIES CXX_MODULE_STD ON)
target_sources(gui PUBLIC 
    FILE_SET modules TYPE CXX_MODULES 
    FILES
        window.cppm
)
target_sources(gui PUBLIC
        window.cpp
)
target_compile_options(gui PUBLIC ${CXX_COMPILE_OPTIONS})
target_link_options(gui PUBLIC ${CXX_LINK_OPTIONS})
target_include_directories(gui PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(gui PUBLIC util Vulkan::Vulkan Qt6::Gui)