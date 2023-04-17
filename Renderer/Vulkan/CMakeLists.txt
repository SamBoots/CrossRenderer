#####################################################
#  This cmakelist handles Vulkan API renderbackend  #
#####################################################
set ( CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON )

#Find vulkan
find_package(Vulkan REQUIRED)
if (NOT Vulkan_FOUND)
message(FATAL_ERROR "Vulkan not found")
else()
message(STATUS "Vulkan found")
message(STATUS ${Vulkan_INCLUDE_DIRS})
endif()

#Add source to this project's executable.
add_library (BB_VulkanDLL SHARED 
"src/VulkanHelperTypes.cpp"
"src/VulkanBackend.cpp"
"src/VulkanCommon.cpp"
"include/VulkanHelperTypes.h"
"include/VulkanCommon.h")

#Executable-Object
set_target_properties ( BB_VulkanDLL PROPERTIES
	PUBLIC_HEADER "include/VulkanBackend.h"
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

#compile Definitions.
if (WIN32)
set(PLATFORM_NAME "_WIN")
target_compile_definitions(BB_VulkanDLL PRIVATE VK_USE_PLATFORM_WIN32_KHR)
elseif(UNIX)
set(PLATFORM_NAME "_LINUX")
target_compile_definitions(BB_VulkanDLL PRIVATE VK_USE_PLATFORM_XLIB_KHR)
endif ()

install(TARGETS BB_VulkanDLL
DESTINATION  $<TARGET_FILE_DIR:Renderer>)