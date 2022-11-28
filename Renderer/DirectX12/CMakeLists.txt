# CMakeList.txt : Cmake project for the DirectX12DLL

# Export all Symbols by default (on Windows creates '.lib'+ '.dll').
set ( CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON )


set (DirectX_DLL_Headers
"include/DX12Backend.h"
"include/DX12Common.h"
#VulkanCommon is included because otherwise Cmake complains about this error on line 20.
#set_target_properties called with incorrect number of arguments. 
#I truly do not know why.
#"include/VulkanCommon.h")
)

set (DirectX_DLL_Source
"src/DX12Backend.cpp"
"src/DX12Common.cpp")


#Add source to this project's executable.
add_library (BB_DirectXDLL SHARED ${DirectX_DLL_Source})

#Executable-Object
set_target_properties ( BB_DirectXDLL PROPERTIES
	PUBLIC_HEADER ${DirectX_DLL_Headers}
	RUNTIME_OUTPUT_DIRECTORY $<TARGET_FILE_DIR:Renderer>
)

#Include library
target_include_directories(BB_DirectXDLL PUBLIC
"../../BB/Framework/include"
"../include/backend"
"include"
)

target_link_libraries(BB_DirectXDLL 
    BBFramework 
    DX12MA
    DX12_ShaderCompiler
    d3d12.lib
    dxgi.lib
    dxguid.lib
    "${CMAKE_SOURCE_DIR}/Libs/DXC/lib/${CPU_ARCHITECTURE}/dxcompiler.lib"
    DX12_ShaderCompiler)

install(TARGETS BB_DirectXDLL
DESTINATION $<TARGET_FILE_DIR:Renderer>)