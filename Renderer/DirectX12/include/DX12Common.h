#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <d3d12.h>
#include <dxgi1_6.h>

//Compile HLSL in runtime for ease of use.
#include <d3dcompiler.h>

#include "RenderBackendCommon.h"

#ifdef _DEBUG
#define DXASSERT(a_HRESULT, a_Msg)\
	if (a_HRESULT != S_OK)\
		BB_ASSERT(false, a_Msg);\

#else
#define DXASSERT(a_HRESULT, a_Msg) a_HRESULT

#endif //_DEBUG

namespace BB
{
	union DX12BufferView
	{
		D3D12_VERTEX_BUFFER_VIEW vertexView;
		D3D12_INDEX_BUFFER_VIEW indexView;
	};

	struct DX12Device
	{
		IDXGIAdapter1* adapter;
		ID3D12Device* logicalDevice;

		ID3D12DebugDevice* debugDevice;
	};

	struct DX12Swapchain
	{
		UINT width;
		UINT height;
		IDXGISwapChain3* swapchain;

		ID3D12DescriptorHeap* rtvHeap;
		ID3D12Resource** renderTargets; //dyn alloc

		D3D12_VIEWPORT viewport;
		D3D12_RECT surfaceRect;
		UINT rtvDescriptorSize;
	};

	//Functions
	BackendInfo DX12CreateBackend(Allocator a_TempAllocator, const RenderBackendCreateInfo& a_CreateInfo);
	FrameBufferHandle DX12CreateFrameBuffer(Allocator a_TempAllocator, const RenderFrameBufferCreateInfo& a_FramebufferCreateInfo);
	RDescriptorHandle DX12CreateDescriptor(Allocator a_TempAllocator, RDescriptorLayoutHandle& a_Layout, const RenderDescriptorCreateInfo& a_CreateInfo);
	PipelineHandle DX12CreatePipeline(Allocator a_TempAllocator, const RenderPipelineCreateInfo& a_CreateInfo);
	CommandListHandle DX12CreateCommandList(Allocator a_TempAllocator, const RenderCommandListCreateInfo& a_CreateInfo);
	RBufferHandle DX12CreateBuffer(const RenderBufferCreateInfo& a_Info);

	RecordingCommandListHandle DX12StartCommandList(const CommandListHandle a_CmdHandle, const FrameBufferHandle a_Framebuffer);
	void DX12ResetCommandList(const CommandListHandle a_CmdHandle);
	void DX12EndCommandList(const RecordingCommandListHandle a_RecordingCmdHandle);
	void DX12BindPipeline(const RecordingCommandListHandle a_RecordingCmdHandle, const PipelineHandle a_Pipeline);
	void DX12BindVertexBuffers(const RecordingCommandListHandle a_RecordingCmdHandle, const RBufferHandle* a_Buffers, const uint64_t* a_BufferOffsets, const uint64_t a_BufferCount);
	void DX12BindIndexBuffer(const RecordingCommandListHandle a_RecordingCmdHandle, const RBufferHandle a_Buffer, const uint64_t a_Offset);
	void DX12BindDescriptorSets(const RecordingCommandListHandle a_RecordingCmdHandle, const uint32_t a_FirstSet, const uint32_t a_SetCount, const RDescriptorHandle* a_Sets, const uint32_t a_DynamicOffsetCount, const uint32_t* a_DynamicOffsets);
	void DX12BindConstant(const RecordingCommandListHandle a_RecordingCmdHandle, const RENDER_SHADER_STAGE a_Stage, const uint32_t a_Offset, const uint32_t a_Size, const void* a_Data);

	void DX12DrawVertex(const RecordingCommandListHandle a_RecordingCmdHandle, const uint32_t a_VertexCount, const uint32_t a_InstanceCount, const uint32_t a_FirstVertex, const uint32_t a_FirstInstance);
	void DX12DrawIndexed(const RecordingCommandListHandle a_RecordingCmdHandle, const uint32_t a_IndexCount, const uint32_t a_InstanceCount, const uint32_t a_FirstIndex, const int32_t a_VertexOffset, const uint32_t a_FirstInstance);
	
	void DX12BufferCopyData(const RBufferHandle a_Handle, const void* a_Data, const uint64_t a_Size, const uint64_t a_Offset);
	void DX12CopyBuffer(Allocator a_TempAllocator, const RenderCopyBufferInfo& a_CopyInfo);
	void* DX12MapMemory(const RBufferHandle a_Handle);
	void DX12UnMemory(const RBufferHandle a_Handle);


	void DX12ResizeWindow(Allocator a_TempAllocator, const uint32_t a_X, const uint32_t a_Y);
	FrameIndex DX12StartFrame();
	void DX12RenderFrame(Allocator a_TempAllocator, const CommandListHandle a_CommandHandle, const FrameBufferHandle a_FrameBufferHandle, const PipelineHandle a_PipeHandle);

	void DX12WaitDeviceReady();

	void DX12DestroyBuffer(const RBufferHandle a_Handle);
	void DX12DestroyDescriptorSetLayout(const RDescriptorLayoutHandle a_Handle);
	void DX12DestroyDescriptorSet(const RDescriptorHandle a_Handle);
	void DX12DestroyCommandList(const CommandListHandle a_Handle);
	void DX12DestroyFramebuffer(const FrameBufferHandle a_Handle);
	void DX12DestroyPipeline(const PipelineHandle a_Handle);
	void DX12DestroyBackend();
}