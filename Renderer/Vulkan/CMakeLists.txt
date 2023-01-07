# CMakeList.txt : Cmake project for the VulkanDLL

set ( CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON )

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
"src/VulkanHelperTypes.cpp"
"src/VulkanBackend.cpp"
"src/VulkanCommon.cpp"
"include/VulkanHelperTypes.h"
"include/VulkanInitializers.h"
"include/VulkanCommon.h")


#Add source to this project's executable.
add_library (BB_VulkanDLL SHARED ${Vulkan_DLL_Source})

set (Vulkan_DLL_Headers
"include/VulkanBackend.h")

#Executable-Object
set_target_properties ( BB_VulkanDLL PROPERTIES
	PUBLIC_HEADER ${Vulkan_DLL_Headers}
	RUNTIME_OUTPUT_DIRECTORY $<TARGET_FILE_DIR:Renderer>
)

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