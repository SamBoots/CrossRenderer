########################################################
#  This cmakelist handles Directx12 API renderbackend  #
########################################################

#Add source to this project's executable.
add_library (BB_DirectX INTERFACE
"src/DX12Backend.cpp"
"src/DX12Common.cpp"
"src/DX12HelperTypes.cpp")

#Include library
target_include_directories(BB_DirectX PUBLIC
"../../BB/Framework/include"
"../include/backend"
"include"
)

target_link_libraries(BB_DirectX
    BBFramework 
    DX12MA
    d3d12.lib
    dxgi.lib
    dxguid.lib)