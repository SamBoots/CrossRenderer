#pragma once
#include "RenderFrontendCommon.h"
#include "Slice.h"

namespace BB
{
	constexpr const size_t MAX_TEXTURES = 1024;

	struct Render_IO
	{
		//IN


		//IN/OUT
		uint32_t swapchainWidth = 0;
		uint32_t swapchainHeight = 0;

		//OUT
		uint32_t frameBufferAmount = 0;
		RENDER_API renderAPI = RENDER_API::NONE;

		RDescriptor globalDescriptor;
		DescriptorAllocation globalDescAllocation;
	};

	//THREAD SAFE: TRUE
	class RenderQueue
	{
	public:
		RenderQueue(const RENDER_QUEUE_TYPE a_QueueType, const char* a_Name);
		~RenderQueue();

		CommandList* GetCommandList(const char* a_ListName = "");

		//Also handles incrementing the 
		void ExecuteCommands(CommandList** a_CommandLists, const uint32_t a_CommandListCount, const RenderFence* a_WaitFences, const RENDER_PIPELINE_STAGE* a_WaitStages, const uint32_t a_FenceCount);
		void ExecutePresentCommands(CommandList** a_CommandLists, const uint32_t a_CommandListCount, const RenderFence* a_WaitFences, const RENDER_PIPELINE_STAGE* a_WaitStages, const uint32_t a_FenceCount);
		void WaitFenceValue(const uint64_t a_FenceValue);
		void WaitIdle();

		const CommandQueueHandle GetQueue() const { return m_Queue; }
		RenderFence GetFence() const { return m_Fence; }
		uint64_t GetNextFenceValue() const { return m_Fence.nextFenceValue; }
		uint64_t GetLastCompletedValue() const { return m_Fence.lastCompleteValue; }

	private:
		const RENDER_QUEUE_TYPE m_Type;
		BBMutex m_Mutex;
		CommandList m_Lists[12]{};
		CommandList* m_FreeCommandList;
		CommandList* m_InFlightLists = nullptr;

		CommandQueueHandle m_Queue;
		RenderFence m_Fence;
	};

	namespace Render
	{
		Render_IO& GetIO();

		void InitRenderer(const RenderInitInfo& a_InitInfo);
		void DestroyRenderer();

		RDescriptorHeap GetGPUHeap(const uint32_t a_FrameNum);
		DescriptorAllocation AllocateDescriptor(const RDescriptor a_Descriptor);
		void UploadDescriptorsToGPU(const uint32_t a_FrameNum);
		RenderBufferPart AllocateFromVertexBuffer(const size_t a_Size);
		RenderBufferPart AllocateFromIndexBuffer(const size_t a_Size);
		const LinearRenderBuffer& GetIndexBuffer();

		const RTexture SetupTexture(const RImageHandle a_Image);
		void FreeTextures(const RTexture* a_Textures, const uint32_t a_Count);
		
		const RDescriptor GetGlobalDescriptorSet();

		RenderQueue& GetGraphicsQueue();
		RenderQueue& GetComputeQueue();
		RenderQueue& GetTransferQueue();

		Model& GetModel(const RModelHandle a_Handle);
		RModelHandle CreateRawModel(const CommandListHandle a_CommandList, const CreateRawModelInfo& a_CreateInfo);
		RModelHandle LoadModel(const CommandListHandle a_CommandList, const LoadModelInfo& a_LoadInfo);

		void StartFrame(const CommandListHandle a_CommandList);
		void Update(const float a_DeltaTime);
		void EndFrame(const CommandListHandle a_CommandList);

		const LightHandle AddLights(const BB::Slice<Light> a_Lights, const LIGHT_TYPE a_LightType);

		void ResizeWindow(const uint32_t a_X, const uint32_t a_Y);
	};
}