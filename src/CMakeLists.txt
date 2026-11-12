add_subdirectory(emulator)
add_subdirectory(interfaces)
add_subdirectory(memory)
add_subdirectory(processor)
add_subdirectory(rom)
add_subdirectory(gui)
add_subdirectory(util)

add_executable(n64emu MACOSX_BUNDLE main.cpp)
set_target_properties(n64emu PROPERTIES CXX_MODULE_STD ON)
target_compile_options(n64emu PUBLIC ${CXX_COMPILE_OPTIONS})
target_include_directories(n64emu PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_options(n64emu PUBLIC ${CXX_LINK_OPTIONS})
target_link_libraries(n64emu PUBLIC stdc++exp emulator gui rdp_gfx_backend)

if(APPLE)
    find_program(BREW_EXECUTABLE brew)
    if(NOT BREW_EXECUTABLE)
        message(FATAL_ERROR "Homebrew is required to configure the macOS app bundle environment")
    endif()

    execute_process(
        COMMAND "${BREW_EXECUTABLE}" --prefix vulkan-validationlayers
        OUTPUT_VARIABLE N64EMU_VALIDATION_LAYERS_PREFIX
        OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE N64EMU_VALIDATION_LAYERS_PREFIX_RESULT
        ERROR_VARIABLE N64EMU_VALIDATION_LAYERS_PREFIX_ERROR
    )
    if(NOT "${N64EMU_VALIDATION_LAYERS_PREFIX_RESULT}" STREQUAL "0")
        message(FATAL_ERROR "Could not resolve the Homebrew Vulkan validation layers prefix: ${N64EMU_VALIDATION_LAYERS_PREFIX_ERROR}")
    endif()

    execute_process(
        COMMAND "${BREW_EXECUTABLE}" --prefix vulkan-loader
        OUTPUT_VARIABLE N64EMU_VULKAN_LOADER_PREFIX
        OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE N64EMU_VULKAN_LOADER_PREFIX_RESULT
        ERROR_VARIABLE N64EMU_VULKAN_LOADER_PREFIX_ERROR
    )
    if(NOT "${N64EMU_VULKAN_LOADER_PREFIX_RESULT}" STREQUAL "0")
        message(FATAL_ERROR "Could not resolve the Homebrew Vulkan loader prefix: ${N64EMU_VULKAN_LOADER_PREFIX_ERROR}")
    endif()

    set(N64EMU_DYLD_LIBRARY_PATH "${N64EMU_VALIDATION_LAYERS_PREFIX}/lib")
    set(N64EMU_QT_VULKAN_LIB "${N64EMU_VULKAN_LOADER_PREFIX}/lib/libvulkan.1.dylib")
    set_target_properties(n64emu PROPERTIES
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.pkbeam.n64emu"
        MACOSX_BUNDLE_BUNDLE_NAME "n64emu"
        MACOSX_BUNDLE_BUNDLE_VERSION "1"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "1.0"
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist.in"
    )
endif()

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