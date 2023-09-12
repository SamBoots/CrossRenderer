###################################################
#  This cmakelist handles all external libraries  #
###################################################
cmake_minimum_required (VERSION 3.8)

#header Libs
add_library(VMA INTERFACE
"VMA/vk_mem_alloc.h")
target_include_directories(VMA INTERFACE VMA)

add_library(DX12MA
"DX12MA/D3D12MemAlloc.h"
"DX12MA/D3D12MemAlloc.cpp")
target_include_directories(DX12MA INTERFACE DX12MA)

add_library(DXCompiler INTERFACE
"DXC/inc/dxcapi.h"
"DXC/inc/d3d12shader.h")
target_include_directories(DXCompiler INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

add_library(stb_image INTERFACE
"stb/stb_image.h"
)

add_library(cgltf INTERFACE
"cgltf/cgltf.h")

add_library(DX12_agility_sdk INTERFACE
"agilitySDK/include/d3dx12/d3dx12.h"
"agilitySDK/src/d3dx12/d3dx12_property_format_table.cpp")
target_include_directories(DX12_agility_sdk INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

add_subdirectory("imgui")