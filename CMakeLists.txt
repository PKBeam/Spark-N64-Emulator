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
set(CXX_COMPILE_OPTIONS
    -freflection
    -Wall
    -Wextra
    -Wpedantic
    -Werror
    -Wno-unused-variable
    -Wno-unused-parameter
    -Wno-unused-but-set-variable
    -fcontracts
    -fmodules
)
set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CXX_SCAN_FOR_MODULES ON)
set(CXX_MODULE_STD ON)

set(CMAKE_CONFIGURATION_TYPES "Debug;Release;RelWithDebInfo;MinSizeRel" CACHE STRING "Supported build types" FORCE)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# workaround to allow std::meta in the std module 
set(CMAKE_CXX_FLAGS "-freflection ${CMAKE_CXX_FLAGS}")

# TODO cross platform compatibility
set(CMAKE_CXX_COMPILER "/usr/bin/g++-16")

# Add AddressSanitizer to Debug builds
if(CMAKE_BUILD_TYPE)
    string(TOUPPER ${CMAKE_BUILD_TYPE} BUILD_TYPE_UPPER)
    if(BUILD_TYPE_UPPER STREQUAL "DEBUG")
        add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
        add_link_options(-fsanitize=address)
    endif()
endif()

add_subdirectory(src)
