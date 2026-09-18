# Spark

A Nintendo 64 emulator written in modern C++.

This is an educational project.  
The goal is to eventually implement an emulator capable of playing retail games at full speed with reasonable visual accuracy.

## Current Status

Able to boot into a game and perform basic rendering with Vulkan.

![Ocarina of Time - Spinning N64 logo](./data/image1.jpg)
![Ocarina of Time - Title Screen cutscene](./data/image2.jpg)
![Ocarina of Time - Title Screen logo](./data/image3.jpg)

## Building

This project requires GCC 16 as it uses modules, contracts and reflection (C++26).  
Some or all of these features are not yet available in Clang/MSVC.

Vulkan 1.4 is required.

## Ubuntu 26.04
Install from apt:
- gcc-16
- cmake=4.2.3
- ninja-build
- libvulkan-dev
- vulkan-tools
- vulkan-validationlayers
- qt6-base-dev
- qt6-wayland
- glslc