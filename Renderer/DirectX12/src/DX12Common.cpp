#include "DX12Backend.h"
#include "DX12Common.h"
#include "D3D12MemAlloc.h"

#include "DXC/inc/dxcapi.h"
#include "DXC/inc/d3d12shader.h"

#include "Slotmap.h"
#include "Pool.h"
#include "BBString.h"

//Tutorial used for this DX12 backend was https://alain.xyz/blog/raw-directx12 

using namespace BB;

static FreelistAllocator_t s_DX12Allocator{ mbSize * 2 };

//Safely releases a type by setting it back to null
void DX12Release(IUnknown* a_Obj)
{
	if (a_Obj)
		a_Obj->Release();
}

namespace DXConv
{
	const D3D12_RESOURCE_STATES ResourceStates(const RENDER_BUFFER_USAGE a_Usage)
	{
		switch (a_Usage)
		{
		case RENDER_BUFFER_USAGE::VERTEX:
			return D3D12_RESOURCE_STATE_COPY_DEST;
			break;
		case RENDER_BUFFER_USAGE::INDEX:
			return D3D12_RESOURCE_STATE_COPY_DEST;
			break;
		case RENDER_BUFFER_USAGE::STORAGE:
			return D3D12_RESOURCE_STATE_COPY_DEST;
			break;
		case RENDER_BUFFER_USAGE::STAGING:
			return D3D12_RESOURCE_STATE_GENERIC_READ;
			break;
		default:
			BB_ASSERT(false, "DX12, Buffer Usage not supported by DX12!");
			return D3D12_RESOURCE_STATE_COMMON;
			break;
		}
	}

	const D3D12_HEAP_TYPE HeapType(const RENDER_MEMORY_PROPERTIES a_Properties)
	{
		switch (a_Properties)
		{
		case RENDER_MEMORY_PROPERTIES::DEVICE_LOCAL:
			return D3D12_HEAP_TYPE_DEFAULT;
			break;
		case RENDER_MEMORY_PROPERTIES::HOST_VISIBLE:
			return D3D12_HEAP_TYPE_UPLOAD;
			break;
		default:
			BB_ASSERT(false, "DX12: Tried to make a commandlist with a queue type that does not exist.");
			return D3D12_HEAP_TYPE_DEFAULT;
			break;
		}
	}

	const D3D12_COMMAND_LIST_TYPE CommandListType(const RENDER_QUEUE_TYPE a_RenderQueueType)
	{
		switch (a_RenderQueueType)
		{
		case RENDER_QUEUE_TYPE::GRAPHICS:
			return D3D12_COMMAND_LIST_TYPE_DIRECT;
			break;
		case RENDER_QUEUE_TYPE::TRANSFER_COPY:
			return D3D12_COMMAND_LIST_TYPE_COPY;
			break;
		default:
			BB_ASSERT(false, "DX12: Tried to make a commandlist with a queue type that does not exist.");
			return D3D12_COMMAND_LIST_TYPE_DIRECT;
			break;
		}
	}
}

struct DXMAResource
{
	D3D12MA::Allocation* allocation;
	ID3D12Resource* resource;
	DX12BufferView view;
};

struct ShaderCompiler
{
	IDxcUtils* utils;
	IDxcCompiler3* compiler;
	IDxcLibrary* library;
};

struct Fence
{
	UINT64 fenceValue = 0;
	ID3D12Fence* fence{};

	bool IsFenceComplete() const
	{
		return fence->GetCompletedValue() >= fenceValue;
	}

	void WaitFence(const uint64_t t_FenceValue)
	{
		if (fence->GetCompletedValue() < fenceValue)
		{
			HANDLE t_Event = ::CreateEvent(NULL, FALSE, FALSE, NULL);
			BB_ASSERT(t_Event, "DX12, failed to create a fence event.");

			DXASSERT(fence->SetEventOnCompletion(t_FenceValue, t_Event),
				"DX12, failed to set event on completion");

			::CloseHandle(t_Event);
		}
	}
};

struct DXCommandAllocator;

struct DXCommandList
{
	union 
	{
		ID3D12GraphicsCommandList* list;
	};

	DXCommandAllocator* allocator;
};

void ResetCommandLists(DXCommandList* a_CommandLists, const uint32_t a_Count)
{
	for (uint32_t i = 0; i < a_Count; i++)
	{
		a_CommandLists[i].list->Reset(a_CommandLists->allocator->allocator, 0);
	}
}

struct DX12Backend_inst
{
	FrameIndex currentFrame = 0;
	UINT backBufferCount = 3; //for now hardcode 3 backbuffers.

	IDXGIFactory4* factory{};
	ID3D12Debug1* debugController{};

	ID3D12CommandQueue* directQueue{};
	ID3D12CommandQueue* copyQueue{};

	ID3D12DescriptorHeap* constantHeap;

	DX12Device device{};
	DX12Swapchain swapchain{};

	D3D12MA::Allocator* DXMA;


	Slotmap<ID3D12RootSignature*> rootSignatures{ s_DX12Allocator };
	Slotmap<ID3D12PipelineState*> pipelines{ s_DX12Allocator };

	Pool<DXCommandAllocator> cmdAllocators;
	Pool<DXMAResource> renderResources;
	Pool<Fence> fencePool;

	ShaderCompiler shaderCompiler;

	void CreatePools()
	{
		cmdAllocators.CreatePool(s_DX12Allocator, 16);
		renderResources.CreatePool(s_DX12Allocator, 8);
		fencePool.CreatePool(s_DX12Allocator, 16);
	}

	void DestroyPools()
	{
		cmdAllocators.DestroyPool(s_DX12Allocator);
		renderResources.DestroyPool(s_DX12Allocator);
		fencePool.DestroyPool(s_DX12Allocator);
	}
};

constexpr uint64_t COMMAND_BUFFER_STANDARD_COUNT = 32;
struct DXCommandAllocator
{
	ID3D12CommandAllocator* allocator;
	D3D12_COMMAND_LIST_TYPE type;
	Pool<DXCommandList> lists;

	DXCommandList* GetCommandList()
	{
		DXCommandList* t_CommandList = lists.Get();
		DXASSERT(s_DX12BackendInst.device.logicalDevice->CreateCommandList(0, 
			type, 
			allocator, 
			nullptr,
			IID_PPV_ARGS(&t_CommandList->list)),
			"DX12: Failed to allocate commandlist.");

		t_CommandList->allocator = this;

		return t_CommandList;
	}
	void FreeCommandList(DXCommandList* t_CmdList)
	{
		t_CmdList->list->Release();
		lists.Free(t_CmdList);
	}
};

static DX12Backend_inst s_DX12BackendInst;

static void SetupShaderCompiler()
{
	DXASSERT(DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&s_DX12BackendInst.shaderCompiler.utils)), 
		"DX12: Failed to create Shader Compile Instance");
	DXASSERT(DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&s_DX12BackendInst.shaderCompiler.compiler)),
		"DX12: Failed to create Shader Compile Compiler");
	DXASSERT(DxcCreateInstance(CLSID_DxcLibrary, IID_PPV_ARGS(& s_DX12BackendInst.shaderCompiler.library)),
		"DX12: Failed to create Shader Compile include_header");
}

enum class ShaderType
{
	VERTEX,
	PIXEL
};

static IDxcBlob* CompileShader(Allocator a_TempAllocator, const wchar_t* a_FullPath, ShaderType a_Type)
{
	LPCWSTR shaderType;
	switch (a_Type)
	{
	case ShaderType::VERTEX:
		shaderType = L"vs_6_0";
		break;
	case ShaderType::PIXEL:
		shaderType = L"ps_6_0";
		break;
	}

	LPCWSTR pszArgs[] =
	{
		a_FullPath,					 // Optional shader source file name for error reporting
									 // and for PIX shader source view.  
		L"-E", L"main",              // Entry point.
		L"-T", shaderType,            // Target.
		L"-Zs",                      // Enable debug information (slim format)
	//	L"-D", L"MYDEFINE=1",        // A single define.
	//	L"-Fo", L"myshader.bin",     // Optional. Stored in the pdb. 
	//	L"-Fd", L"myshader.pdb",     // The file name of the pdb. This must either be supplied
									 // or the autogenerated file name must be used.
	//	L"-Qstrip_reflect",          // Strip reflection into a separate blob. 
	};

	IDxcBlobEncoding* t_SourceBlob;
	DXASSERT(s_DX12BackendInst.shaderCompiler.utils->LoadFile(a_FullPath, nullptr, &t_SourceBlob),
		"DX12: Failed to load file before compiling it.");
	DxcBuffer t_Source;
	t_Source.Ptr = t_SourceBlob->GetBufferPointer();
	t_Source.Size = t_SourceBlob->GetBufferSize();
	t_Source.Encoding = DXC_CP_ACP;

	IDxcResult* t_Result;
	HRESULT t_HR;

	t_HR = s_DX12BackendInst.shaderCompiler.compiler->Compile(
		&t_Source,
		pszArgs,
		_countof(pszArgs),
		nullptr,
		IID_PPV_ARGS(&t_Result)
	);

	IDxcBlobUtf8* t_Errors = nullptr;
	t_Result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&t_Errors), nullptr);

	if (t_Errors != nullptr)
	{
		wprintf(L"Shader Compilation failed with errors:\n%hs\n",
			(const char*)t_Errors->GetStringPointer());
		t_Errors->Release();
	}
	
	t_Result->GetStatus(&t_HR);
	if (FAILED(t_HR))
	{
		BB_ASSERT(false, "DX12: Failed to load shader.");
	}

	IDxcBlob* t_ShaderCode;
	t_Result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&t_ShaderCode), nullptr);
	if (t_ShaderCode->GetBufferPointer() == nullptr)
	{
		BB_ASSERT(false, "DX12: Something went wrong with DXC shader compiling.");
	}

	t_SourceBlob->Release();
	t_Result->Release();

	return t_ShaderCode;
}

static void SetupBackendSwapChain(UINT a_Width, UINT a_Height, HWND a_WindowHandle)
{
	s_DX12BackendInst.swapchain.width = a_Width;
	s_DX12BackendInst.swapchain.height = a_Height;

	//Just do a resize if a swapchain already exists.
	if (s_DX12BackendInst.swapchain.swapchain != nullptr)
	{
		s_DX12BackendInst.swapchain.swapchain->ResizeBuffers(s_DX12BackendInst.backBufferCount,
			a_Width,
			a_Height,
			DXGI_FORMAT_R8G8B8A8_UNORM,
			0);
		return;
	}

	D3D12_VIEWPORT t_Viewport;
	D3D12_RECT t_SurfaceRect;

	t_Viewport.TopLeftX = 0.0f;
	t_Viewport.TopLeftY = 0.0f;
	t_Viewport.Width = static_cast<float>(a_Width);
	t_Viewport.Height = static_cast<float>(a_Height);
	t_Viewport.MinDepth = .1f;
	t_Viewport.MaxDepth = 1000.f;

	t_SurfaceRect.left = 0;
	t_SurfaceRect.top = 0;
	t_SurfaceRect.right = static_cast<LONG>(a_Width);
	t_SurfaceRect.bottom = static_cast<LONG>(a_Height);

	s_DX12BackendInst.swapchain.viewport = t_Viewport;
	s_DX12BackendInst.swapchain.surfaceRect = t_SurfaceRect;

	DXGI_SWAP_CHAIN_DESC1 t_SwapchainDesc = {};
	t_SwapchainDesc.BufferCount = s_DX12BackendInst.backBufferCount;
	t_SwapchainDesc.Width = a_Width;
	t_SwapchainDesc.Height = a_Height;
	t_SwapchainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	t_SwapchainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	t_SwapchainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	t_SwapchainDesc.SampleDesc.Count = 1;

	IDXGISwapChain1* t_NewSwapchain;
	DXASSERT(s_DX12BackendInst.factory->CreateSwapChainForHwnd(
		s_DX12BackendInst.directQueue,
		a_WindowHandle,
		&t_SwapchainDesc,
		nullptr,
		nullptr,
		&t_NewSwapchain), 
		"DX12: Failed to create swapchain1");

	DXASSERT(s_DX12BackendInst.factory->MakeWindowAssociation(a_WindowHandle,
		DXGI_MWA_NO_ALT_ENTER),
		"DX12: Failed to add DXGI_MWA_NO_ALT_ENTER to window.");

	DXASSERT(t_NewSwapchain->QueryInterface(
		__uuidof(IDXGISwapChain3), (void**)&t_NewSwapchain),
		"DX12: Failed to get support for a IDXGISwapchain3.");

	s_DX12BackendInst.swapchain.swapchain = (IDXGISwapChain3*)t_NewSwapchain;

	s_DX12BackendInst.currentFrame = s_DX12BackendInst.swapchain.swapchain->GetCurrentBackBufferIndex();

	D3D12_DESCRIPTOR_HEAP_DESC t_RtvHeapDesc = {};
	t_RtvHeapDesc.NumDescriptors = s_DX12BackendInst.backBufferCount;
	t_RtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	t_RtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	DXASSERT(s_DX12BackendInst.device.logicalDevice->CreateDescriptorHeap(
		&t_RtvHeapDesc, IID_PPV_ARGS(&s_DX12BackendInst.swapchain.rtvHeap)),
		"DX12: Failed to create descriptor heap for swapchain.");


	s_DX12BackendInst.swapchain.rtvDescriptorSize =
		s_DX12BackendInst.device.logicalDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = 
		s_DX12BackendInst.swapchain.rtvHeap->GetCPUDescriptorHandleForHeapStart();

	s_DX12BackendInst.swapchain.renderTargets = BBnewArr(s_DX12Allocator, s_DX12BackendInst.backBufferCount, ID3D12Resource*);

	// Create a RTV for each frame.
	for (UINT i = 0; i < s_DX12BackendInst.backBufferCount; i++)
	{
		DXASSERT(s_DX12BackendInst.swapchain.swapchain->GetBuffer(i,
			IID_PPV_ARGS(&s_DX12BackendInst.swapchain.renderTargets[i])),
			"DX12: Failed to get swapchain buffer.");

		s_DX12BackendInst.device.logicalDevice->CreateRenderTargetView(
			s_DX12BackendInst.swapchain.renderTargets[i], 
			nullptr, 
			rtvHandle);
		rtvHandle.ptr += (1 * s_DX12BackendInst.swapchain.rtvDescriptorSize);
	}
}


BackendInfo BB::DX12CreateBackend(Allocator a_TempAllocator, const RenderBackendCreateInfo& a_CreateInfo)
{
	UINT t_FactoryFlags = 0;
	s_DX12BackendInst.CreatePools();

	if (a_CreateInfo.validationLayers)
	{
		ID3D12Debug* t_DebugController;
		DXASSERT(D3D12GetDebugInterface(IID_PPV_ARGS(&t_DebugController)),
			"DX12: failed to create debuginterface.");
		DXASSERT(t_DebugController->QueryInterface(IID_PPV_ARGS(&s_DX12BackendInst.debugController)),
			"DX12: failed to create debuginterface.");
		s_DX12BackendInst.debugController->EnableDebugLayer();
		s_DX12BackendInst.debugController->SetEnableGPUBasedValidation(true);

		t_FactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;

		t_DebugController->Release();
	}
	else
	{
		s_DX12BackendInst.debugController = nullptr;
	}

	DXASSERT(CreateDXGIFactory2(t_FactoryFlags, IID_PPV_ARGS(&s_DX12BackendInst.factory)),
		"DX12: failed to create DXGIFactory2.");

#pragma region DEVICE_CREATION
	IDXGIAdapter1* t_CurrentBestAdapter = nullptr;
	SIZE_T t_BestDedicatedVRAM = 0;

	for (UINT adapterIndex = 0;
		DXGI_ERROR_NOT_FOUND != s_DX12BackendInst.factory->EnumAdapters1(adapterIndex, &s_DX12BackendInst.device.adapter);
		++adapterIndex)
	{
		DXGI_ADAPTER_DESC1 t_Desc;
		s_DX12BackendInst.device.adapter->GetDesc1(&t_Desc);

		//We don't take the software adapter.
		if (t_Desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
			continue;

		if (t_Desc.DedicatedVideoMemory > t_BestDedicatedVRAM)
		{
			t_BestDedicatedVRAM = t_Desc.DedicatedVideoMemory;
			t_CurrentBestAdapter = s_DX12BackendInst.device.adapter;
		}
	}
	s_DX12BackendInst.device.adapter = t_CurrentBestAdapter;

	DXASSERT(D3D12CreateDevice(s_DX12BackendInst.device.adapter,
		D3D_FEATURE_LEVEL_12_0,
		IID_PPV_ARGS(&s_DX12BackendInst.device.logicalDevice)),
		"DX12: Failed to create logical device.");

	if (a_CreateInfo.validationLayers)
	{
		DXASSERT(s_DX12BackendInst.device.logicalDevice->QueryInterface(&s_DX12BackendInst.device.debugDevice),
			"DX12: Failed to query debug device.");
	}
	else
	{
		s_DX12BackendInst.device.debugDevice = nullptr;
	}
#pragma endregion //DEVICE_CREATION

	D3D12MA::ALLOCATOR_DESC t_AllocatorDesc = {};
	t_AllocatorDesc.pDevice = s_DX12BackendInst.device.logicalDevice;
	t_AllocatorDesc.pAdapter = s_DX12BackendInst.device.adapter;

	DXASSERT(D3D12MA::CreateAllocator(&t_AllocatorDesc, &s_DX12BackendInst.DXMA),
		"DX12: Failed to create DX12 memory allocator");

	D3D12_COMMAND_QUEUE_DESC t_QueueDesc{};
	t_QueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	t_QueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	DXASSERT(s_DX12BackendInst.device.logicalDevice->CreateCommandQueue(&t_QueueDesc,
		IID_PPV_ARGS(&s_DX12BackendInst.directQueue)),
		"DX12: Failed to create direct command queue");

	t_QueueDesc.Type = D3D12_COMMAND_LIST_TYPE_COPY;

	DXASSERT(s_DX12BackendInst.device.logicalDevice->CreateCommandQueue(&t_QueueDesc,
		IID_PPV_ARGS(&s_DX12BackendInst.copyQueue)),
		"DX12: Failed to create direct command queue");

	SetupBackendSwapChain(a_CreateInfo.windowWidth, a_CreateInfo.windowHeight, a_CreateInfo.hwnd);

	//Create the shader compiler.
	SetupShaderCompiler();

	//Returns some info to the global backend that is important.
	BackendInfo t_BackendInfo;
	t_BackendInfo.currentFrame = s_DX12BackendInst.currentFrame;
	t_BackendInfo.framebufferCount = s_DX12BackendInst.backBufferCount;

	return t_BackendInfo;
}

RDescriptorHandle BB::DX12CreateDescriptor(Allocator a_TempAllocator, RDescriptorLayoutHandle& a_Layout, const RenderDescriptorCreateInfo& a_CreateInfo)
{
	if (a_Layout.ptrHandle != nullptr)
	{
		return RDescriptorHandle(a_Layout.ptrHandle);
	}

	ID3D12RootSignature* t_RootSignature = nullptr;

	D3D12_FEATURE_DATA_ROOT_SIGNATURE t_FeatureData = {};
	t_FeatureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;

	if (FAILED(s_DX12BackendInst.device.logicalDevice->CheckFeatureSupport(
		D3D12_FEATURE_ROOT_SIGNATURE,
		&t_FeatureData, sizeof(t_FeatureData))))
	{
		t_FeatureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
	}

	D3D12_ROOT_CONSTANTS* t_RootConstants = BBnewArr(
		a_TempAllocator,
		a_CreateInfo.constantBinds.size(),
		D3D12_ROOT_CONSTANTS);

	UINT t_RegisterSpace = 0;
	for (size_t i = 0; i < a_CreateInfo.constantBinds.size(); i++)
	{
		BB_ASSERT(a_CreateInfo.constantBinds[i].size % sizeof(uint32_t) == 0, "DX12: BindConstant a_size is not a multiple of 32!");
		const UINT t_Dwords = a_CreateInfo.constantBinds[i].size / sizeof(uint32_t);
		t_RootConstants[i].Num32BitValues = t_Dwords;
		t_RootConstants[i].ShaderRegister = t_RegisterSpace++;
		t_RootConstants[i].RegisterSpace = 0; //We will just keep this 0 for now.
	}


	D3D12_ROOT_DESCRIPTOR1* t_RootDescriptor = BBnewArr(
		a_TempAllocator,
		a_CreateInfo.bufferBinds.size(),
		D3D12_ROOT_DESCRIPTOR1);

	for (size_t i = 0; i < a_CreateInfo.bufferBinds.size(); i++)
	{
		t_RootDescriptor[i].RegisterSpace = 0; //We will keep this 0 for now.
		t_RootDescriptor[i].ShaderRegister = t_RegisterSpace++;
		t_RootDescriptor[i].Flags = D3D12_ROOT_DESCRIPTOR_FLAG_NONE;
	}

	//Groups of GPU Resources
	D3D12_ROOT_PARAMETER1 t_RootParameters[2]{};
	t_RootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	t_RootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL; //This is for the indices so make it visible to all.
	t_RootParameters[0].Descriptor = t_RootDescriptor[0];

	t_RootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
	t_RootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL; //This is for the indices so make it visible to all.
	t_RootParameters[1].Constants = t_RootConstants[0];

	//Overall Layout
	D3D12_VERSIONED_ROOT_SIGNATURE_DESC t_RootSignatureDesc;
	t_RootSignatureDesc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
	t_RootSignatureDesc.Desc_1_1.Flags =
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	t_RootSignatureDesc.Desc_1_1.NumParameters = 2;
	t_RootSignatureDesc.Desc_1_1.pParameters = t_RootParameters;
	t_RootSignatureDesc.Desc_1_1.NumStaticSamplers = 0;
	t_RootSignatureDesc.Desc_1_1.pStaticSamplers = nullptr;

	ID3DBlob* t_Signature;
	ID3DBlob* t_Error;

	D3D12SerializeVersionedRootSignature(&t_RootSignatureDesc,
		&t_Signature, &t_Error);

	if (t_Error != nullptr)
	{
		BB_LOG((const char*)t_Error->GetBufferPointer());
		BB_ASSERT(false, "DX12: error creating root signature, details are above.");
		t_Error->Release();
	}

	DXASSERT(s_DX12BackendInst.device.logicalDevice->CreateRootSignature(0,
		t_Signature->GetBufferPointer(),
		t_Signature->GetBufferSize(),
		IID_PPV_ARGS(&t_RootSignature)),
		"DX12: Failed to create root signature.");

	t_RootSignature->SetName(L"Hello Triangle Root Signature");

	if (t_Signature != nullptr)
		t_Signature->Release();

	a_Layout.ptrHandle = t_RootSignature;
	return RDescriptorHandle(t_RootSignature);
}

PipelineHandle BB::DX12CreatePipeline(Allocator a_TempAllocator, const RenderPipelineCreateInfo& a_CreateInfo)
{
	ID3D12PipelineState* t_PipelineState;

	D3D12_GRAPHICS_PIPELINE_STATE_DESC t_PsoDesc = {};

	D3D12_INPUT_ELEMENT_DESC t_InputElementDescs[] = {
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
		D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12,
		D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0} };

	t_PsoDesc.InputLayout = { t_InputElementDescs, _countof(t_InputElementDescs) };

	t_PsoDesc.pRootSignature = reinterpret_cast<ID3D12RootSignature*>(a_CreateInfo.descLayoutHandles[0].ptrHandle);
	IDxcBlob* t_ShaderCode[2]; // 0 vertex, 1 frag.
	t_ShaderCode[0] = CompileShader(a_TempAllocator, a_CreateInfo.shaderPaths[0], ShaderType::VERTEX);
	t_ShaderCode[1] = CompileShader(a_TempAllocator, a_CreateInfo.shaderPaths[1], ShaderType::PIXEL);

	//All the vertex shader stuff
	D3D12_SHADER_BYTECODE t_VertexShader;
	t_VertexShader.BytecodeLength = t_ShaderCode[0]->GetBufferSize();
	t_VertexShader.pShaderBytecode = t_ShaderCode[0]->GetBufferPointer();
	t_PsoDesc.VS = t_VertexShader;

	//All the pixel shader stuff
	D3D12_SHADER_BYTECODE t_PixelShader;
	t_PixelShader.BytecodeLength = t_ShaderCode[1]->GetBufferSize();
	t_PixelShader.pShaderBytecode = t_ShaderCode[1]->GetBufferPointer();
	t_PsoDesc.PS = t_PixelShader;

	D3D12_RASTERIZER_DESC t_RasterDesc;
	t_RasterDesc.FillMode = D3D12_FILL_MODE_SOLID;
	t_RasterDesc.CullMode = D3D12_CULL_MODE_NONE;
	t_RasterDesc.FrontCounterClockwise = FALSE;
	t_RasterDesc.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
	t_RasterDesc.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
	t_RasterDesc.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
	t_RasterDesc.DepthClipEnable = TRUE;
	t_RasterDesc.MultisampleEnable = FALSE;
	t_RasterDesc.AntialiasedLineEnable = FALSE;
	t_RasterDesc.ForcedSampleCount = 0;
	t_RasterDesc.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
	t_PsoDesc.RasterizerState = t_RasterDesc;
	t_PsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

	D3D12_BLEND_DESC t_BlendDesc;
	t_BlendDesc.AlphaToCoverageEnable = FALSE;
	t_BlendDesc.IndependentBlendEnable = FALSE;
	const D3D12_RENDER_TARGET_BLEND_DESC defaultRenderTargetBlendDesc = {
		FALSE,
		FALSE,
		D3D12_BLEND_ONE,
		D3D12_BLEND_ZERO,
		D3D12_BLEND_OP_ADD,
		D3D12_BLEND_ONE,
		D3D12_BLEND_ZERO,
		D3D12_BLEND_OP_ADD,
		D3D12_LOGIC_OP_NOOP,
		D3D12_COLOR_WRITE_ENABLE_ALL,
	};
	for (UINT i = 0; i < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i)
		t_BlendDesc.RenderTarget[i] = defaultRenderTargetBlendDesc;
	t_PsoDesc.BlendState = t_BlendDesc;

	t_PsoDesc.DepthStencilState.DepthEnable = FALSE;
	t_PsoDesc.DepthStencilState.StencilEnable = FALSE;
	t_PsoDesc.SampleMask = UINT_MAX;

	t_PsoDesc.NumRenderTargets = 1;
	t_PsoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	t_PsoDesc.SampleDesc.Count = 1;

	DXASSERT(s_DX12BackendInst.device.logicalDevice->CreateGraphicsPipelineState(
		&t_PsoDesc, IID_PPV_ARGS(&t_PipelineState)),
		"DX12: Failed to create graphics pipeline");

	return PipelineHandle(s_DX12BackendInst.pipelines.emplace(t_PipelineState).handle);
}

CommandListHandle BB::DX12CreateCommandList(Allocator a_TempAllocator, const RenderCommandListCreateInfo& a_CreateInfo)
{
	return CommandListHandle(reinterpret_cast<DXCommandAllocator*>(a_CreateInfo.commandAllocator.ptrHandle)->GetCommandList());
}

RBufferHandle BB::DX12CreateBuffer(const RenderBufferCreateInfo& a_Info)
{
	DXMAResource* t_Resource = s_DX12BackendInst.renderResources.Get();

	D3D12_RESOURCE_DESC t_ResourceDesc = {};
	t_ResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	t_ResourceDesc.Alignment = 0;
	t_ResourceDesc.Width = a_Info.size;
	t_ResourceDesc.Height = 1;
	t_ResourceDesc.DepthOrArraySize = 1;
	t_ResourceDesc.MipLevels = 1;
	t_ResourceDesc.Format = DXGI_FORMAT_UNKNOWN;
	t_ResourceDesc.SampleDesc.Count = 1;
	t_ResourceDesc.SampleDesc.Quality = 0;
	t_ResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	t_ResourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	D3D12MA::ALLOCATION_DESC t_AllocationDesc = {};
	//t_AllocationDesc.HeapType = DXConv::HeapType(a_Info.memProperties);
	t_AllocationDesc.HeapType = D3D12_HEAP_TYPE_UPLOAD;
	D3D12_RESOURCE_STATES t_States = D3D12_RESOURCE_STATE_COPY_DEST;

	//if (a_Info.usage == RENDER_BUFFER_USAGE::STAGING)
		t_States = D3D12_RESOURCE_STATE_GENERIC_READ;

	DXASSERT(s_DX12BackendInst.DXMA->CreateResource(
		&t_AllocationDesc,
		&t_ResourceDesc,
		t_States,
		NULL,
		&t_Resource->allocation,
		IID_PPV_ARGS(&t_Resource->resource)),
		"DX12: Failed to create resource using D3D12 Memory Allocator");

	switch (a_Info.usage)
	{
	case RENDER_BUFFER_USAGE::VERTEX:
		t_Resource->view.vertexView.BufferLocation = t_Resource->resource->GetGPUVirtualAddress();
		t_Resource->view.vertexView.StrideInBytes = sizeof(Vertex);
		t_Resource->view.vertexView.SizeInBytes = static_cast<UINT>(a_Info.size);
		break;
	case RENDER_BUFFER_USAGE::INDEX:
		t_Resource->view.indexView.BufferLocation = t_Resource->resource->GetGPUVirtualAddress();
		t_Resource->view.indexView.Format = DXGI_FORMAT_R32_UINT;
		t_Resource->view.indexView.SizeInBytes = static_cast<UINT>(a_Info.size);
		break;
	default:
		t_Resource->view.constantView.BufferLocation = t_Resource->resource->GetGPUVirtualAddress();
		t_Resource->view.constantView.SizeInBytes = static_cast<UINT>(a_Info.size);
		break;
	}

	if (a_Info.data != nullptr)
	{
		void* t_MappedPtr;
		D3D12_RANGE t_ReadRange;
		t_ReadRange.Begin = 0;
		t_ReadRange.End = 0;

		DXASSERT(t_Resource->resource->Map(0, nullptr, &t_MappedPtr),
			"DX12: failed to map memory to resource.");
		memcpy(t_MappedPtr, a_Info.data, a_Info.size);
		t_Resource->resource->Unmap(0, nullptr);
	}

	return RBufferHandle(t_Resource);
}

RSemaphoreHandle BB::DX12CreateSemaphore()
{
	//not sure what semaphores in DX12 are yet, don't think they have em the same way.
}

RFenceHandle BB::DX12CreateFence(const FenceCreateInfo& a_Info)
{
	Fence* t_Fence = s_DX12BackendInst.fencePool.Get();
	s_DX12BackendInst.device.logicalDevice->CreateFence(0,
		D3D12_FENCE_FLAG_NONE,
		IID_PPV_ARGS(&t_Fence->fence));
	return RFenceHandle(t_Fence);
}


RecordingCommandListHandle BB::DX12StartCommandList(const CommandListHandle a_CmdHandle, const FrameBufferHandle a_Framebuffer)
{
	DXCommandList* t_CommandList = reinterpret_cast<DXCommandList*>(a_CmdHandle.ptrHandle);
	return RecordingCommandListHandle(t_CommandList);
}

void BB::DX12ResetCommandList(const CommandListHandle a_CmdHandle)
{
	DXCommandList* t_CommandList = reinterpret_cast<DXCommandList*>(a_CmdHandle.ptrHandle);
	DXASSERT(t_CommandList->list->Reset(t_CommandList->allocator->allocator, nullptr)
		, "DX12: Failed to reset commandlist.");
}

void BB::DX12EndCommandList(const RecordingCommandListHandle a_RecordingCmdHandle)
{
	ID3D12GraphicsCommandList* t_CommandList = reinterpret_cast<ID3D12GraphicsCommandList*>(a_RecordingCmdHandle.ptrHandle);
	//Close it for now
	t_CommandList->Close();
}

void BB::DX12BindPipeline(const RecordingCommandListHandle a_RecordingCmdHandle, const PipelineHandle a_Pipeline)
{
	ID3D12GraphicsCommandList* t_CommandList = reinterpret_cast<ID3D12GraphicsCommandList*>(a_RecordingCmdHandle.ptrHandle);
	t_CommandList->SetPipelineState(s_DX12BackendInst.pipelines.find(a_Pipeline.handle));
}

void BB::DX12BindVertexBuffers(const RecordingCommandListHandle a_RecordingCmdHandle, const RBufferHandle* a_Buffers, const uint64_t* a_BufferOffsets, const uint64_t a_BufferCount)
{
	ID3D12GraphicsCommandList* t_CommandList = reinterpret_cast<ID3D12GraphicsCommandList*>(a_RecordingCmdHandle.ptrHandle);
	D3D12_VERTEX_BUFFER_VIEW t_Views[12]{};
	for (size_t i = 0; i < a_BufferCount; i++)
	{
		t_Views[i] = reinterpret_cast<DXMAResource*>(a_Buffers[i].ptrHandle)->view.vertexView;
	}

	t_CommandList->IASetVertexBuffers(0, static_cast<uint32_t>(a_BufferCount), t_Views);
}

void BB::DX12BindIndexBuffer(const RecordingCommandListHandle a_RecordingCmdHandle, const RBufferHandle a_Buffer, const uint64_t a_Offset)
{
	ID3D12GraphicsCommandList* t_CommandList = reinterpret_cast<ID3D12GraphicsCommandList*>(a_RecordingCmdHandle.ptrHandle);
	t_CommandList->IASetIndexBuffer(&reinterpret_cast<DXMAResource*>(a_Buffer.ptrHandle)->view.indexView);
}


void BB::DX12BindDescriptorSets(const RecordingCommandListHandle a_RecordingCmdHandle, const uint32_t a_FirstSet, const uint32_t a_SetCount, const RDescriptorHandle* a_Sets, const uint32_t a_DynamicOffsetCount, const uint32_t* a_DynamicOffsets)
{
	ID3D12GraphicsCommandList* t_CommandList = reinterpret_cast<ID3D12GraphicsCommandList*>(a_RecordingCmdHandle.ptrHandle);
	
	t_CommandList->SetGraphicsRootSignature(reinterpret_cast<ID3D12RootSignature*>(a_Sets[0].ptrHandle));
}

void BB::DX12BindConstant(const RecordingCommandListHandle a_RecordingCmdHandle, const RENDER_SHADER_STAGE a_Stage, const uint32_t a_Offset, const uint32_t a_Size, const void* a_Data)
{
	ID3D12GraphicsCommandList* t_CommandList = reinterpret_cast<ID3D12GraphicsCommandList*>(a_RecordingCmdHandle.ptrHandle);

	BB_ASSERT(a_Size % sizeof(uint32_t) == 0, "DX12: BindConstant a_size is not a multiple of 32!");
	const UINT t_Dwords = a_Size / sizeof(uint32_t);
	t_CommandList->SetGraphicsRoot32BitConstants(0, t_Dwords, a_Data, a_Offset);
}

void BB::DX12DrawVertex(const RecordingCommandListHandle a_RecordingCmdHandle, const uint32_t a_VertexCount, const uint32_t a_InstanceCount, const uint32_t a_FirstVertex, const uint32_t a_FirstInstance)
{
	ID3D12GraphicsCommandList* t_CommandList = reinterpret_cast<ID3D12GraphicsCommandList*>(a_RecordingCmdHandle.ptrHandle);

	t_CommandList->DrawInstanced(a_VertexCount, a_InstanceCount, a_FirstInstance, a_InstanceCount);
}

void BB::DX12DrawIndexed(const RecordingCommandListHandle a_RecordingCmdHandle, const uint32_t a_IndexCount, const uint32_t a_InstanceCount, const uint32_t a_FirstIndex, const int32_t a_VertexOffset, const uint32_t a_FirstInstance)
{
	ID3D12GraphicsCommandList* t_CommandList = reinterpret_cast<ID3D12GraphicsCommandList*>(a_RecordingCmdHandle.ptrHandle);

	t_CommandList->DrawIndexedInstanced(a_IndexCount, a_InstanceCount, a_FirstIndex, a_VertexOffset, a_FirstInstance);
}


void BB::DX12BufferCopyData(const RBufferHandle a_Handle, const void* a_Data, const uint64_t a_Size, const uint64_t a_Offset)
{
	DXMAResource* t_Resource = reinterpret_cast<DXMAResource*>(a_Handle.ptrHandle);
	void* t_MapData;

	DXASSERT(t_Resource->resource->Map(0, NULL, &t_MapData),
		"DX12: Failed to map resource.");

	memcpy(Pointer::Add(t_MapData, a_Offset), a_Data, a_Size);

	t_Resource->resource->Unmap(0, NULL);
}

void BB::DX12CopyBuffer(Allocator a_TempAllocator, const RenderCopyBufferInfo& a_CopyInfo)
{
	ID3D12GraphicsCommandList* t_CommandList = reinterpret_cast<ID3D12GraphicsCommandList*>(a_CopyInfo.transferCommandHandle.ptrHandle);

	ID3D12Resource* t_DestResource = reinterpret_cast<DXMAResource*>(a_CopyInfo.dst.ptrHandle)->resource;

	for (size_t i = 0; i < a_CopyInfo.CopyRegionCount; i++)
	{
		t_CommandList->CopyBufferRegion(
			t_DestResource,
			a_CopyInfo.copyRegions[i].dstOffset,
			reinterpret_cast<DXMAResource*>(a_CopyInfo.src.handle)->resource,
			a_CopyInfo.copyRegions[i].srcOffset,
			a_CopyInfo.copyRegions[i].size);
	}

	ID3D12CommandList* t_CommandListSend[] = { t_CommandList };
	s_DX12BackendInst.copyQueue->ExecuteCommandLists(1, t_CommandListSend);
}

struct RenderBarriersInfo
{
	struct ResourceBarriers
	{
		RENDER_BUFFER_USAGE previous;
		RENDER_BUFFER_USAGE next;
		RBufferHandle buffer;
		uint32_t subresource;
	};
	ResourceBarriers* barriers;
	uint32_t barrierCount;
};

void DX12ResourceBarrier(Allocator a_TempAllocator, const RecordingCommandListHandle a_RecordingCmdHandle, const RenderBarriersInfo& a_Info)
{
	D3D12_RESOURCE_BARRIER* t_ResourceBarriers = BBnewArr(a_TempAllocator, 
		a_Info.barrierCount,
		D3D12_RESOURCE_BARRIER);

	for (size_t i = 0; i < a_Info.barrierCount; i++)
	{
		t_ResourceBarriers[i].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		t_ResourceBarriers[i].Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		t_ResourceBarriers[i].Transition.pResource = reinterpret_cast<DXMAResource*>(a_Info.barriers[i].buffer.ptrHandle)->resource;
		t_ResourceBarriers[i].Transition.StateBefore = DXConv::ResourceStates(a_Info.barriers[i].previous);
		t_ResourceBarriers[i].Transition.StateAfter = DXConv::ResourceStates(a_Info.barriers[i].next);
		t_ResourceBarriers[i].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	}

	ID3D12GraphicsCommandList* t_CommandList = reinterpret_cast<ID3D12GraphicsCommandList*>(a_RecordingCmdHandle.ptrHandle);
	t_CommandList->ResourceBarrier(a_Info.barrierCount, t_ResourceBarriers);
}


void* BB::DX12MapMemory(const RBufferHandle a_Handle)
{
	DXMAResource* t_Resource = reinterpret_cast<DXMAResource*>(a_Handle.ptrHandle);
	void* t_MapData;
	DXASSERT(t_Resource->resource->Map(0, NULL, &t_MapData),
		"DX12: Failed to map resource.");
	return t_MapData;
}

void BB::DX12UnMemory(const RBufferHandle a_Handle)
{
	DXMAResource* t_Resource = reinterpret_cast<DXMAResource*>(a_Handle.ptrHandle);
	t_Resource->resource->Unmap(0, NULL);
}

void BB::DX12StartFrame(Allocator a_TempAllocator, const StartFrameInfo& a_StartInfo)
{

}

void BB::DX12ExecuteGraphicCommands(Allocator a_TempAllocator, const ExecuteCommandsInfo* a_ExecuteInfos, const uint32_t a_ExecuteInfoCount, RFenceHandle a_SumbitFence)
{
	for (size_t i = 0; i < a_ExecuteInfoCount; i++)
	{
		ID3D12CommandList** t_CommandLists = BBnewArr(
			a_TempAllocator,
			a_ExecuteInfos[i].commandCount,
			ID3D12CommandList*
		);

		for (size_t j = 0; j < a_ExecuteInfos[i].commandCount; j++)
		{
			t_CommandLists[j] = reinterpret_cast<ID3D12CommandList*>(
				a_ExecuteInfos[i].commands[j].ptrHandle);
		}

		s_DX12BackendInst.directQueue->ExecuteCommandLists(
			a_ExecuteInfos[i].commandCount, 
			t_CommandLists);

		ResetCommandLists(reinterpret_cast<DXCommandList*>(a_ExecuteInfos[i].commands), a_ExecuteInfos[i].commandCount);
	}
}

void BB::DX12ExecuteTransferCommands(Allocator a_TempAllocator, const ExecuteCommandsInfo* a_ExecuteInfos, const uint32_t a_ExecuteInfoCount, RFenceHandle a_SumbitFence)
{
	for (size_t i = 0; i < a_ExecuteInfoCount; i++)
	{
		ID3D12CommandList** t_CommandLists = BBnewArr(
			a_TempAllocator,
			a_ExecuteInfos[i].commandCount,
			ID3D12CommandList*
		);

		for (size_t j = 0; j < a_ExecuteInfos[j].commandCount; j++)
		{
			t_CommandLists[j] = reinterpret_cast<DXCommandList*>(
				a_ExecuteInfos[i].commands[j].ptrHandle)->list;
		}

		s_DX12BackendInst.copyQueue->ExecuteCommandLists(
			a_ExecuteInfos[i].commandCount,
			t_CommandLists);

		ResetCommandLists(reinterpret_cast<DXCommandList*>(a_ExecuteInfos[i].commands), a_ExecuteInfos[i].commandCount);
	}
}

FrameIndex BB::DX12PresentFrame(Allocator a_TempAllocator, const PresentFrameInfo& a_PresentInfo)
{


	s_DX12BackendInst.swapchain.swapchain->Present(1, 0);
	s_DX12BackendInst.currentFrame = s_DX12BackendInst.swapchain.swapchain->GetCurrentBackBufferIndex();

	return s_DX12BackendInst.currentFrame;
}



void BB::DX12RenderFrame(Allocator a_TempAllocator, const CommandListHandle a_CommandHandle, const FrameBufferHandle a_FrameBufferHandle, const PipelineHandle a_PipeHandle)
{
	DXCommandList& t_CommandList = s_DX12BackendInst.commandLists.find(a_CommandHandle.ptrHandle);

	D3D12_RESOURCE_BARRIER renderTargetBarrier;
	renderTargetBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	renderTargetBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	renderTargetBarrier.Transition.pResource = s_DX12BackendInst.swapchain.renderTargets[s_DX12BackendInst.currentFrame];
	renderTargetBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
	renderTargetBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
	renderTargetBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	t_CommandList.list->ResourceBarrier(1, &renderTargetBarrier);

	D3D12_CPU_DESCRIPTOR_HANDLE
		rtvHandle(s_DX12BackendInst.swapchain.rtvHeap->GetCPUDescriptorHandleForHeapStart());
	rtvHandle.ptr = rtvHandle.ptr + (s_DX12BackendInst.currentFrame * s_DX12BackendInst.swapchain.rtvDescriptorSize);
	t_CommandList.list->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

	const float clearColor[] = { 0.2f, 0.2f, 0.2f, 1.0f };
	t_CommandList.list->RSSetViewports(1, &s_DX12BackendInst.swapchain.viewport);
	t_CommandList.list->RSSetScissorRects(1, &s_DX12BackendInst.swapchain.surfaceRect);
	t_CommandList.list->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
	t_CommandList.list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	t_CommandList.list->IASetVertexBuffers(0, 1, &s_DX12BackendInst.renderResources.find(0).view.vertexView);

	//t_CommandList->DrawInstanced(3, 1, 0, 0);
	DX12DrawVertex(t_CommandList.list, 3, 1, 0, 0);

	// Indicate that the back buffer will now be used to present.
	D3D12_RESOURCE_BARRIER presentBarrier;
	presentBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	presentBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	presentBarrier.Transition.pResource = s_DX12BackendInst.swapchain.renderTargets[s_DX12BackendInst.currentFrame];
	presentBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
	presentBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
	presentBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

	t_CommandList.list->ResourceBarrier(1, &presentBarrier);

	DXASSERT(t_CommandList.list->Close(), "DX12: Failed to close commandlist.");

	ID3D12CommandList* t_CommandListSend[] = { t_CommandList.list };
	s_DX12BackendInst.directQueue->ExecuteCommandLists(1, t_CommandListSend);

	s_DX12BackendInst.swapchain.swapchain->Present(1, 0);
	s_DX12BackendInst.currentFrame = s_DX12BackendInst.swapchain.swapchain->GetCurrentBackBufferIndex();
}

void BB::DX12WaitDeviceReady()
{
	const UINT64 fenceV = s_DX12BackendInst.fenceValue;
	BB_WARNING(false, "DX12: DX12WaitDeviceReady function is unfinished, using it is dangerous.", WarningType::MEDIUM);
	//if (s_DX12BackendInst.fence->GetCompletedValue() < fenceV)
	//{
	//	DXASSERT(s_DX12BackendInst.fence->SetEventOnCompletion(fenceV, s_DX12BackendInst.fenceEvent),
	//		"DX12: Failed to wait for event complection on fence.");
	//	WaitForSingleObject(s_DX12BackendInst.fenceEvent, INFINITE);
	//}
}

void BB::DX12DestroyFence(const RFenceHandle a_Handle)
{
	DX12Release(reinterpret_cast<Fence*>(a_Handle.ptrHandle)->fence);
	s_DX12BackendInst.fencePool.Free(reinterpret_cast<Fence*>(a_Handle.ptrHandle));
}

void BB::DX12DestroySemaphore(const RSemaphoreHandle a_Handle)
{

}

void BB::DX12DestroyBuffer(const RBufferHandle a_Handle)
{
	DXMAResource* t_Resource = reinterpret_cast<DXMAResource*>(a_Handle.ptrHandle);
	t_Resource->resource->Release();
	t_Resource->allocation->Release();
	s_DX12BackendInst.renderResources.Free(t_Resource);
}

void BB::DX12DestroyCommandAllocator(const CommandAllocatorHandle a_Handle)
{
	DXCommandAllocator* t_CmdAllocator = reinterpret_cast<DXCommandAllocator*>(a_Handle.ptrHandle);
	t_CmdAllocator->lists.DestroyPool(s_DX12Allocator);
	t_CmdAllocator->allocator->Release();
	s_DX12BackendInst.cmdAllocators.Free(t_CmdAllocator);
}

void BB::DX12DestroyCommandList(const CommandListHandle a_Handle)
{
	DXCommandList* t_RemovedList = reinterpret_cast<DXCommandList*>(a_Handle.ptrHandle);

	t_RemovedList->list->Release();
	t_RemovedList->allocator->lists.Free(t_RemovedList);
}

void BB::DX12DestroyPipeline(const PipelineHandle a_Handle)
{
	s_DX12BackendInst.pipelines.find(a_Handle.handle)->Release();
	s_DX12BackendInst.pipelines.erase(a_Handle.handle);
}

void BB::DX12DestroyDescriptorSetLayout(const RDescriptorLayoutHandle a_Handle)
{
	reinterpret_cast<ID3D12RootSignature*>(a_Handle.ptrHandle)->Release();
}

void BB::DX12DestroyDescriptorSet(const RDescriptorHandle a_Handle)
{

}

void BB::DX12DestroyBackend()
{
	for (size_t i = 0; i < s_DX12BackendInst.backBufferCount; i++)
	{
		s_DX12BackendInst.swapchain.renderTargets[i]->Release();
	}
	BBfreeArr(s_DX12Allocator, s_DX12BackendInst.swapchain.renderTargets);
	s_DX12BackendInst.swapchain.rtvHeap->Release();
	s_DX12BackendInst.swapchain.swapchain->SetFullscreenState(false, NULL);
	s_DX12BackendInst.swapchain.swapchain->Release();
	s_DX12BackendInst.swapchain.swapchain = nullptr;

	s_DX12BackendInst.directQueue->Release();
	s_DX12BackendInst.copyQueue->Release();

	s_DX12BackendInst.DXMA->Release();
	if (s_DX12BackendInst.device.debugDevice)
		s_DX12BackendInst.device.debugDevice->Release();
	
	s_DX12BackendInst.device.logicalDevice->Release();
	s_DX12BackendInst.device.adapter->Release();
	if (s_DX12BackendInst.debugController)
		s_DX12BackendInst.debugController->Release();
	s_DX12BackendInst.factory->Release();
}