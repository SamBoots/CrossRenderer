#Cmake for all the third_party libraries, they will be compiled or collected here.
cmake_minimum_required (VERSION 3.8)

#header Libs
add_library(VMA INTERFACE)
target_include_directories(VMA INTERFACE VMA)

add_library(DX12MA
"DX12MA/D3D12MemAlloc.h"
"DX12MA/D3D12MemAlloc.cpp")
target_include_directories(DX12MA INTERFACE DX12MA)

add_library(DX12_ShaderCompiler INTERFACE
"DXC/include/dxc/dxcapi.h")
target_include_directories(DX12_ShaderCompiler INTERFACE DX12_ShaderCompiler)

set(DXCOMPILER_DLL "${CMAKE_SOURCE_DIR}/Libs/DXC/bin/dxcompiler.dll")
set(DXCOMPILER_LIB "${CMAKE_SOURCE_DIR}/Libs/DXC/lib/dxcompiler.lib")