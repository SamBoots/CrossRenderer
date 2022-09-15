# CMakeList.txt : CMake project for Allocator, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)
set(ENV{BASH_ENV} "/home/pi/.bashrc")
execute_process(COMMAND bash-c "")

#compile Definitions.
if (WIN32)
set(PLATFORM_NAME "_WIN")
add_compile_definitions(PUBLIC VK_USE_PLATFORM_WIN32_KHR)
elseif(UNIX)
set(PLATFORM_NAME "_LINUX")
add_compile_definitions(PUBLIC VK_USE_PLATFORM_XLIB_KHR)
endif ()

#Add source to this project's executable.
add_executable (Renderer 
"include/Backend/RenderBackend.h"
"include/Backend/BackendCommands.h"
"include/Backend/VK/VulkanBackend.h"
"include/Backend/VK/VulkanInitializers.h"
"include/Backend/VK/VulkanCommon.h"
"include/Backend/DX12/DX12Common.h"
"include/Backend/DX12/DX12Backend.h"

"src/Backend/RenderBackend.cpp"
"src/Backend/VK/VulkanBackend.cpp"
"src/Backend/DX12/DX12Backend.cpp"
"src/main.cpp")

find_package(Vulkan REQUIRED)
if (NOT Vulkan_FOUND)
message(FATAL_ERROR "Vulkan not found")
else()
message(STATUS "Vulkan found")
message(STATUS ${Vulkan_INCLUDE_DIRS})
endif()
target_link_libraries(Renderer Vulkan::Vulkan)

#Include library
target_include_directories(Renderer PUBLIC
${Vulkan_INCLUDE_DIRS}
"../BB/Framework/include"
"include"
"include/Backend"
"include/Backend/VK"
"include/Backend/DX12"
"include/Frontend"
)

target_link_libraries(Renderer 
    BBFramework 
    ${VULKAN_LIB_LIST} 
    d3d12.lib
    dxgi.lib
    dxguid.lib)