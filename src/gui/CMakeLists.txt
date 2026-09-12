add_library(gui)
set_target_properties(gui PROPERTIES CXX_MODULE_STD ON)
target_sources(gui PUBLIC 
    FILE_SET modules TYPE CXX_MODULES 
    FILES
        window.cppm
)
target_sources(gui PUBLIC
        window.cpp
        vkRenderWindow.cpp
)
target_compile_options(gui PUBLIC ${CXX_COMPILE_OPTIONS})
target_link_options(gui PUBLIC ${CXX_LINK_OPTIONS})
target_include_directories(gui PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(gui PUBLIC rdp_gfx_backend util Vulkan::Vulkan Qt6::Gui)

set(SHADER_SOURCES "window.vert" "window.frag")
foreach(SHADER_SOURCE IN LISTS SHADER_SOURCES)
get_filename_component(SHADER_NAME ${CMAKE_CURRENT_SOURCE_DIR}/${SHADER_SOURCE} NAME_WE)
  add_custom_target(gui_gfx_shaders_${SHADER_SOURCE} ALL
    COMMAND Vulkan::glslc ${CMAKE_CURRENT_SOURCE_DIR}/${SHADER_SOURCE} -o ${CMAKE_CURRENT_BINARY_DIR}/../${SHADER_SOURCE}.spv  --target-env=vulkan1.4
    COMMENT "Compiling GLSL shader: [${SHADER_SOURCE}]"
    SOURCES ${SHADER_SOURCE}
    BYPRODUCTS "${CMAKE_CURRENT_BINARY_DIR}/../${SHADER_SOURCE}.spv"
  )
endforeach()