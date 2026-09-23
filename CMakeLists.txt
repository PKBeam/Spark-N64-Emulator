cmake_minimum_required(VERSION 4.2)

if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.30.0")
    if(CMAKE_VERSION VERSION_LESS "3.31.8")
        set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD
            "0e5b6991-d74f-4b3d-a41c-cf096e0b2508"
        )
    elseif(CMAKE_VERSION VERSION_LESS "4.0.0")
        set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD
            "d0edc3af-4c50-42ea-a356-e2862fe7a444"
        )
    elseif(CMAKE_VERSION VERSION_LESS "4.0.3")
        set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD
            "a9e1cf81-9932-4810-974b-6eccaf14e457"
        )
    elseif(CMAKE_VERSION VERSION_LESS "4.3.0")
        set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD
            "d0edc3af-4c50-42ea-a356-e2862fe7a444"
        )
    elseif(CMAKE_VERSION VERSION_LESS "4.4.0")
        set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD
            "451f2fe2-a8a2-47c3-bc32-94786d8fc91b"
        )
    endif()
endif()

project(n64emu LANGUAGES CXX)

set(CMAKE_CONFIGURATION_TYPES "Debug;Release;RelWithDebInfo;Profiling" CACHE STRING "Supported build types" FORCE)

set(CXX_COMPILE_OPTIONS
    -freflection
    -Wall
    -Wextra
    -Wpedantic
    -Werror
    -Wno-unused-variable
    -Wno-unused-parameter
    -Wno-unused-but-set-variable
    -Wno-unused-but-set-parameter
    -fcontracts
    -fmodules
    -fconcepts-diagnostics-depth=3
    -fvar-tracking-assignments
)

set(CXX_LINK_OPTIONS
    "-Wl,--allow-multiple-definition") # 'import std;' causes issues with multiple definitions with GCC

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CXX_SCAN_FOR_MODULES ON)
set(CXX_MODULE_STD ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CONFIGURATION_TYPES "Debug;Release;RelWithDebInfo;Profiling" CACHE STRING "Supported build types" FORCE)
set(CMAKE_CXX_FLAGS_PROFILING "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -fno-omit-frame-pointer")

# workaround to allow std::meta in the std module 
set(CMAKE_CXX_FLAGS "-freflection ${CMAKE_CXX_FLAGS}")

add_compile_options(
    "$<$<CONFIG:Debug>:-ggdb3;-g3>"
    "$<$<CONFIG:RelWithDebInfo>:-ggdb3;-g3>"
)
if(UNIX)
add_compile_options(
    "$<$<CONFIG:Debug>:-fsanitize=address;-fsanitize=undefined;-fsanitize=thread;-fno-omit-frame-pointer>"
)
add_link_options(
    "$<$<CONFIG:Debug>:-fsanitize=address;-fsanitize=undefined;-fsanitize=thread>"
)
endif()
add_compile_definitions(
    "$<$<CONFIG:Debug>:ENABLE_LOGS;ENABLE_VK_VALIDATION;DETERMINISTIC_VI_INTERRUPTS>"
    "$<$<CONFIG:RelWithDebInfo>:ENABLE_LOGS;ENABLE_VK_VALIDATION>"
)

find_package(Vulkan REQUIRED COMPONENTS glslc)
find_package(Qt6 REQUIRED COMPONENTS Gui)

add_subdirectory(src)
