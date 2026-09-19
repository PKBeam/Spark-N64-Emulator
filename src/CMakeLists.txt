add_subdirectory(emulator)
add_subdirectory(interfaces)
add_subdirectory(memory)
add_subdirectory(processor)
add_subdirectory(rom)
add_subdirectory(gui)
add_subdirectory(util)

add_executable(n64emu main.cpp)
set_target_properties(n64emu PROPERTIES CXX_MODULE_STD ON)
target_compile_options(n64emu PUBLIC ${CXX_COMPILE_OPTIONS})
target_include_directories(n64emu PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_options(n64emu PUBLIC ${CXX_LINK_OPTIONS})
target_link_libraries(n64emu PUBLIC stdc++exp emulator gui rdp_gfx_backend)

add_custom_command(
    TARGET n64emu POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/data $<TARGET_FILE_DIR:n64emu>/data
    COMMENT "Copying data directory to build output"
)

set(SHADER_SOURCES "$<TARGET_FILE_DIR:gui>/../window.vert.spv" "$<TARGET_FILE_DIR:gui>/../window.frag.spv" "$<TARGET_FILE_DIR:rdp_gfx_backend>/../rdp.vert.spv" "$<TARGET_FILE_DIR:rdp_gfx_backend>/../rdp.frag.spv")

foreach(SHADER_SOURCE IN LISTS SHADER_SOURCES)
    add_custom_command(
        TARGET n64emu POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
            ${SHADER_SOURCE} $<TARGET_FILE_DIR:n64emu>/
        COMMENT "Copying shader: [${SHADER_SOURCE}] to build output"
    )
endforeach()