########################################################
#  This cmakelist handles Directx12 API renderbackend  #
########################################################

# Export all Symbols by default (on Windows creates '.lib'+ '.dll').
set ( CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON )

#Add source to this project's executable.
add_library (BB_DirectXDLL SHARED
"src/DX12Backend.cpp"
"src/DX12Common.cpp"
"src/DX12HelperTypes.cpp")

#Executable-Object
set_target_properties ( BB_DirectXDLL PROPERTIES
	PUBLIC_HEADER "include/DX12Backend.h"
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
    d3d12.lib
    dxgi.lib
    dxguid.lib
    DX12_agility_sdk)

add_custom_command(TARGET BB_DirectXDLL POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
        "${CMAKE_SOURCE_DIR}/Libs/AgilitySDK/bin/${CPU_ARCHITECTURE}/D3D12Core.dll"
        $<TARGET_FILE_DIR:BB_DirectXDLL>)

add_custom_command(TARGET BB_DirectXDLL POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
        "${CMAKE_SOURCE_DIR}/Libs/AgilitySDK/bin/${CPU_ARCHITECTURE}/d3d12SDKLayers.dll"
        $<TARGET_FILE_DIR:BB_DirectXDLL>)