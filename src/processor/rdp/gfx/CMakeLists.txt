
add_library(rdp_gfx_backend)
set_target_properties(rdp_gfx_backend PROPERTIES CXX_MODULE_STD ON)
target_sources(rdp_gfx_backend PUBLIC 
  FILE_SET HEADERS TYPE HEADERS FILES 
  FILES
    gfxBackend.hpp
    vulkanBackend.hpp
)
target_sources(rdp_gfx_backend PUBLIC
  gfxBackend.cpp
  vulkanBackend.cpp
)
target_compile_options(rdp_gfx_backend PUBLIC ${CXX_COMPILE_OPTIONS})
target_link_options(rdp_gfx_backend PUBLIC ${CXX_LINK_OPTIONS})
target_include_directories(rdp_gfx_backend PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(rdp_gfx_backend PUBLIC util Vulkan::Vulkan Qt6::Core Qt6::Gui)

set(SHADER_SOURCES "rdp.vert" "rdp.frag")
set(SHADER_OUTPUTS)
foreach(SHADER_SOURCE IN LISTS SHADER_SOURCES)
  set(SHADER_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/../${SHADER_SOURCE}.spv")
  add_custom_command(
    OUTPUT "${SHADER_OUTPUT}"
    COMMAND Vulkan::glslc ${CMAKE_CURRENT_SOURCE_DIR}/${SHADER_SOURCE} -o ${CMAKE_CURRENT_BINARY_DIR}/../${SHADER_SOURCE}.spv --target-env=vulkan1.4
    COMMENT "Compiling GLSL shader: [${SHADER_SOURCE}]"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/${SHADER_SOURCE}"
  )
  list(APPEND SHADER_OUTPUTS "${SHADER_OUTPUT}")
endforeach()
add_custom_target(rdp_gfx_shaders ALL DEPENDS ${SHADER_OUTPUTS})
add_dependencies(rdp_gfx_backend rdp_gfx_shaders)