#Cmake for all the third_party libraries, they will be compiled or collected here.
cmake_minimum_required (VERSION 3.8)

#header Libs
add_library(VMA INTERFACE)
target_include_directories(VMA INTERFACE VMA)

add_library(DX12MA
"DX12MA/D3D12MemAlloc.h"
"DX12MA/D3D12MemAlloc.cpp")
target_include_directories(DX12MA INTERFACE DX12MA)

add_library(DXCompiler INTERFACE
"DXC/inc/dxcapi.h"
"DXC/inc/d3d12shader.h")
target_include_directories(DXCompiler INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

find_package(GLM)
#Not found
if (${GLM_FOUND})
message(STATUS "glm found, version ${GLM_VERSION}")
else()
message(STATUS "glm not found, downloading it form https://github.com/g-truc/glm/releases/download/0.9.9.8/glm-0.9.9.8.zip")
include(FetchContent)
FetchContent_Declare(
  glmLib
  URL https://github.com/g-truc/glm/releases/download/0.9.9.8/glm-0.9.9.8.zip
)
FetchContent_MakeAvailable(glmLib)
message(STATUS "glm downloaded, version ${GLM_VERSION}")
endif()

message(STATUS "GLM Source dir: ${glm_SOURCE_DIR}")