#pragma once
#include "RenderBackendCommon.h"

namespace BB
{
	class Editor;

	enum class RESOURCE_TYPE
	{
		DESCRIPTOR,
		COMMAND_QUEUE,
		COMMAND_ALLOCATOR,
		COMMAND_LIST,

		BUFFER,
		IMAGE,
		SAMPLER,
		FENCE
	};

	struct RenderResource
	{
		RESOURCE_TYPE type;
		union
		{
			RenderDescriptorCreateInfo descriptor;
			RenderCommandQueueCreateInfo queue;
			RenderCommandAllocatorCreateInfo commandAllocator;
			RenderCommandListCreateInfo commandList;
			RenderBufferCreateInfo buffer;
			RenderImageCreateInfo image;
			SamplerCreateInfo sampler;
			FenceCreateInfo fence;
		};
	};
	
	class RenderResourceTracker
	{
	public:
		//editor will display the values that are private here.
		friend class BB::Editor;


		RenderResourceTracker() {};
		~RenderResourceTracker() {};

		void AddResource(const RenderResource& a_Resource);
		void Editor() const;

	private:
		FreelistAllocator_t m_Allocator{ mbSize * 2 };

		Array<RenderResource> m_RenderResource{ m_Allocator, 1024 };	};
}