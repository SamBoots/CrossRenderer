#pragma once
#include "RenderBackendCommon.h"

namespace BB
{
	class Editor;

	enum class RESOURCE_TYPE : uint32_t
	{
		DESCRIPTOR,
		COMMAND_QUEUE,
		COMMAND_ALLOCATOR,
		COMMAND_LIST,
		PIPELINE,
		BUFFER,
		IMAGE,
		SAMPLER,
		FENCE
	};

	struct TrackerImageInfo
	{
		RENDER_IMAGE_LAYOUT oldLayout = RENDER_IMAGE_LAYOUT::UNDEFINED;
		RENDER_IMAGE_LAYOUT currentLayout = RENDER_IMAGE_LAYOUT::UNDEFINED;
		RenderImageCreateInfo createInfo{};
	};

	class RenderResourceTracker
	{
	public:
		friend class BB::Editor;

		RenderResourceTracker();
		~RenderResourceTracker();

		void AddDescriptor(const RenderDescriptorCreateInfo& a_Descriptor, const char* a_Name, const uint64_t a_ID);
		void AddQueue(const RenderCommandQueueCreateInfo& a_Queue, const char* a_Name, const uint64_t a_ID);
		void AddCommandAllocator(const RenderCommandAllocatorCreateInfo& a_CommandAllocator, const char* a_Name, const uint64_t a_ID);
		void AddCommandList(const RenderCommandListCreateInfo& a_CommandList, const char* a_Name, const uint64_t a_ID);
		void AddPipeline(const PipelineDebugInfo& a_Pipeline, const char* a_Name, const uint64_t a_ID);
		void AddBuffer(const RenderBufferCreateInfo& a_Buffer, const char* a_Name, const uint64_t a_ID);
		void AddImage(const TrackerImageInfo& a_Image, const char* a_Name, const uint64_t a_ID);
		void AddSampler(const SamplerCreateInfo& a_Sampler, const char* a_Name, const uint64_t a_ID);
		void AddFence(const FenceCreateInfo& a_Fence, const char* a_Name, const uint64_t a_ID);

		//Get the typedata, 
		void* GetData(const uint64_t a_ID, const RESOURCE_TYPE a_Type);
		void ChangeName(const uint64_t a_ID, const char* a_NewName);

		void RemoveEntry(const uint64_t a_ID);

		void Editor();
		void SortByType();
		void SortByTime();

	private:
		FreelistAllocator_t m_Allocator{ mbSize * 2 };
		struct RenderResourceTracker_Inst* inst = nullptr;
	};
}