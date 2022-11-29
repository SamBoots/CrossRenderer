# CMakeList.txt : Cmake project for the DirectX12DLL

# Export all Symbols by default (on Windows creates '.lib'+ '.dll').
set ( CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON )

set (DirectX_DLL_Source
"include/DX12Backend.h"
"src/DX12Backend.cpp"
"src/DX12Common.cpp")


#Add source to this project's executable.
add_library (BB_DirectXDLL SHARED ${DirectX_DLL_Source})

#Executable-Object
set_target_properties ( BB_DirectXDLL PROPERTIES
	PUBLIC_HEADER "include/DX12Common.h"
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