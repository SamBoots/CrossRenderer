#pragma once
#include "RenderBackendCommon.h"
#include "Common.h"

namespace BB
{
	using FrameGraphResourceHandle = FrameworkHandle<struct FrameGraphResourceHandleTag>;
	using FrameGraphNodeHandle = FrameworkHandle<struct FrameGraphNodeHandleTag>;

	enum class FRAME_RESOURCE
	{
		BUFFER,
		IMAGE
	};

	struct SceneGraph
	{



	};


	typedef void (*PFN_FrameGraphPreRender)(const RecordingCommandListHandle a_Cmd);
	typedef void (*PFN_FrameGraphRender)(const RecordingCommandListHandle a_Cmd);

	struct FrameGraphRenderPass
	{
		PFN_FrameGraphPreRender prerender;
		PFN_FrameGraphRender render;
	};

	struct FrameGraphResource
	{
		FrameGraphNodeHandle producer;
		FRAME_RESOURCE type;
		union
		{
			struct Buffer
			{
				uint64_t size;
				uint64_t offset;
				RBufferHandle buffer;
			} buffer;

			struct Image
			{
				uint32_t width;
				uint32_t height;
				uint16_t depth;
				uint8_t mips;
				uint8_t layers;
				RENDER_IMAGE_FORMAT format;
				RImageHandle image;
			} image;
		}
	};

	struct FrameGraph_inst;
	
	class FrameGraph
	{
	public:
		FrameGraph();
		~FrameGraph();

		const FrameGraphResourceHandle CreateResource(const FrameGraphResource& a_Resource);
		void DestroyResource(const FrameGraphResourceHandle a_Handle);


	private:
		FreelistAllocator_t m_Allocator{ mbSize * 32 };
		FrameGraph_inst* m_Inst;
	};
}