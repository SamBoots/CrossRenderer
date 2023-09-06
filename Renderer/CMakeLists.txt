#####################################################
#  This cmakelist handles the top-level renderer    #
#####################################################

if ("${GRAPHICS_API}" STREQUAL "Vulkan")
add_compile_definitions(PUBLIC USE_VULKAN)
elseif("${GRAPHICS_API}" STREQUAL "DirectX12")
add_compile_definitions(PUBLIC USE_DIRECTX12)
else()
message(FATAL_ERROR "Not chosen a valid graphicsAPI, the one chosen is: ${GraphicsAPI}")
endif()

if (CMAKE_BUILD_TYPE MATCHES Debug)
add_compile_definitions(PUBLIC _TRACK_RENDER_RESOURCES)
elseif(CMAKE_BUILD_TYPE MATCHES Release)
add_compile_definitions(PUBLIC _TRACK_RENDER_RESOURCES)
endif(CMAKE_BUILD_TYPE)

#Add source to this project's executable.
add_executable (Renderer 
#API specific backend
"Vulkan/src/VulkanHelperTypes.cpp"
"Vulkan/src/VulkanBackend.cpp"
"Vulkan/src/VulkanCommon.cpp"

#Backend
"src/Backend/RenderBackend.cpp"
"src/Backend/ShaderCompiler.cpp"
"src/Backend/RenderResourceTracker.cpp"
#Frontend
"src/Frontend/AssetLoader.cpp"
"src/Frontend/RenderFrontend.cpp"
"src/Frontend/LightSystem.cpp"
"src/Frontend/Transform.cpp"
"src/Frontend/Materials.cpp"
"src/Frontend/Camera.cpp"

#graph
"src/Graph/FrameGraph.cpp"
"src/Graph/SceneGraph.cpp"

#Tooling & misc
"src/imgui_impl_CrossRenderer.cpp"
"src/Editor.cpp"

#temporary to test
"src/main.cpp")

#Include library
target_include_directories(Renderer PUBLIC
"../BB/Framework/include"
"include"
"include/Backend"
"include/Frontend"
"include/graph"

"Vulkan/include"
"DirectX12/include"
${glm_SOURCE_DIR}
)

find_package(Vulkan)
if (NOT Vulkan_FOUND)
message(STATUS "Vulkan not found")
else()
message(STATUS "Vulkan found")
message(STATUS ${Vulkan_INCLUDE_DIRS})
target_link_libraries(Renderer
    Vulkan::Vulkan
    VMA
)
endif()

target_link_libraries(Renderer
    DXCompiler
    "${CMAKE_SOURCE_DIR}/Libs/DXC/lib/${CPU_ARCHITECTURE}/dxcompiler.lib"
    BBFramework 
    stb_image
    IMGUI
    #BB_DirectX
)

#compile Definitions.
if (WIN32)
set(PLATFORM_NAME "_WIN")
target_compile_definitions(Renderer PRIVATE VK_USE_PLATFORM_WIN32_KHR)
elseif(UNIX)
set(PLATFORM_NAME "_LINUX")
target_compile_definitions(Renderer PRIVATE VK_USE_PLATFORM_XLIB_KHR)
endif ()

#Executable-Object
set_target_properties (Renderer PROPERTIES
	RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}>)

add_custom_command(TARGET Renderer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
        "${CMAKE_SOURCE_DIR}/Libs/DXC/bin/${CPU_ARCHITECTURE}/dxcompiler.dll"
        $<TARGET_FILE_DIR:Renderer>)

add_custom_command(TARGET Renderer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
        "${CMAKE_SOURCE_DIR}/Libs/DXC/bin/${CPU_ARCHITECTURE}/dxil.dll"
        $<TARGET_FILE_DIR:Renderer>)