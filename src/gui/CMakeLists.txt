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
target_compile_options(gui PUBLIC "--embed-dir=${CMAKE_CURRENT_BINARY_DIR}")
target_link_libraries(gui PUBLIC rdp_gfx_backend util Vulkan::Vulkan Qt6::Gui)

if (APPLE)
set(BRIDGE_SRC "${CMAKE_CURRENT_SOURCE_DIR}/qtAppleClangBridge.cpp")
set(BRIDGE_OBJ "${CMAKE_CURRENT_BINARY_DIR}/qtAppleClangBridge.o")
add_custom_command(
    OUTPUT ${BRIDGE_OBJ}
    COMMAND /usr/bin/clang++ -c ${BRIDGE_SRC} -o ${BRIDGE_OBJ}
    -std=c++20
    -stdlib=libc++
    -F/usr/local/Qt-6.13.0/lib
    -I/opt/homebrew/include
    -isysroot /Library/Developer/CommandLineTools/SDKs/MacOSX.sdk
    DEPENDS ${BRIDGE_SRC}
    COMMENT "Compiling vulkan_qt_bridge.cpp with Apple Clang..."
)
target_sources(gui PRIVATE ${BRIDGE_OBJ})
target_link_libraries(gui PRIVATE c++)
endif()

set(SHADER_SOURCES "window.vert" "window.frag")
set(SHADER_OUTPUTS)
foreach(SHADER_SOURCE IN LISTS SHADER_SOURCES)
  set(SHADER_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${SHADER_SOURCE}.spv")
  add_custom_command(
    OUTPUT "${SHADER_OUTPUT}"
    COMMAND Vulkan::glslc ${CMAKE_CURRENT_SOURCE_DIR}/${SHADER_SOURCE} -o ${SHADER_OUTPUT} --target-env=vulkan1.4
    COMMENT "Compiling GLSL shader: [${SHADER_SOURCE}]"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/${SHADER_SOURCE}"
  )
  list(APPEND SHADER_OUTPUTS "${SHADER_OUTPUT}")
endforeach()
add_custom_target(gui_gfx_shaders ALL DEPENDS ${SHADER_OUTPUTS})
add_dependencies(gui gui_gfx_shaders)