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

#Add source to this project's executable.
add_executable (Renderer 
#Backend .h
"include/Backend/RenderBackend.h"
"include/Backend/RenderBackendCommon.h"
"include/Backend/ShaderCompiler.h"
#Frontend .h
"include/Frontend/RenderFrontendCommon.h"
"include/Frontend/RenderFrontend.h"
"include/Frontend/ModelLoader.h"
"include/Frontend/Transform.h" 

#Backend .cpp
"src/Backend/RenderBackend.cpp"
"src/Backend/ShaderCompiler.cpp"
#Frontend .cpp
"src/Frontend/RenderFrontend.cpp"
"src/Frontend/ModelLoader.cpp"
"src/Frontend/Transform.cpp"

#temporary to test
"src/main.cpp")

target_link_libraries(Renderer
    DXCompiler
    "${CMAKE_SOURCE_DIR}/Libs/DXC/lib/${CPU_ARCHITECTURE}/dxcompiler.lib"
    BBFramework 
    stb_image
)

#Include library
target_include_directories(Renderer PUBLIC
"../BB/Framework/include"
"include"
"include/Backend"
"include/Frontend"

"Vulkan/include"
"DirectX12/include"
${glm_SOURCE_DIR}
)

add_subdirectory("Vulkan")
#add_subdirectory("DirectX12")

add_dependencies(Renderer BB_VulkanDLL)
#add_dependencies(Renderer BB_DirectXDLL)

add_custom_command(TARGET Renderer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
        "${CMAKE_SOURCE_DIR}/Libs/DXC/bin/${CPU_ARCHITECTURE}/dxcompiler.dll"
        $<TARGET_FILE_DIR:Renderer>)

add_custom_command(TARGET Renderer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
        "${CMAKE_SOURCE_DIR}/Libs/DXC/bin/${CPU_ARCHITECTURE}/dxil.dll"
        $<TARGET_FILE_DIR:Renderer>)