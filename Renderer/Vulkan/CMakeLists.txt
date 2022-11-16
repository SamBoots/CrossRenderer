# CMakeList.txt : Cmake project for the VulkanDLL


#Find vulkan
find_package(Vulkan REQUIRED)
if (NOT Vulkan_FOUND)
message(FATAL_ERROR "Vulkan not found")
else()
message(STATUS "Vulkan found")
message(STATUS ${Vulkan_INCLUDE_DIRS})
endif()

#compile Definitions.
if (WIN32)
set(PLATFORM_NAME "_WIN")
add_compile_definitions(PUBLIC VK_USE_PLATFORM_WIN32_KHR)
elseif(UNIX)
set(PLATFORM_NAME "_LINUX")
add_compile_definitions(PUBLIC VK_USE_PLATFORM_XLIB_KHR)
endif ()

set (Vulkan_DLL_Source
"src/VulkanBackend.cpp"
"src/VulkanCommon.cpp")


#Add source to this project's executable.
add_library (BB_VulkanDLL SHARED ${Vulkan_DLL_Source})

# Export all Symbols by default (on Windows creates '.lib'+ '.dll').
if (CMAKE_BUILD_TYPE MATCHES Debug)
set ( CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON )

set (Vulkan_DLL_Headers
"include/VulkanBackend.h"
"include/VulkanInitializers.h"
"include/VulkanCommon.h")

#Executable-Object
set_target_properties ( BB_VulkanDLL PROPERTIES
	PUBLIC_HEADER ${Vulkan_DLL_Headers}
	RUNTIME_OUTPUT_DIRECTORY $<TARGET_FILE_DIR:Renderer>
)
endif(CMAKE_BUILD_TYPE)

#Include library
target_include_directories(BB_VulkanDLL PUBLIC
${Vulkan_INCLUDE_DIRS}
"../../BB/Framework/include"
"../include/backend"
"include"
)

target_link_libraries(BB_VulkanDLL 
    BBFramework 
    Vulkan::Vulkan
    VMA)

install(TARGETS BB_VulkanDLL
DESTINATION  $<TARGET_FILE_DIR:Renderer>)