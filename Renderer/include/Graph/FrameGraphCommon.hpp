#pragma once
#include "RenderFrontendCommon.h"

namespace BB
{
	using FrameGraphResourceHandle = FrameworkHandle<struct FrameGraphResourceHandleTag>;
	using FrameGraphNodeHandle = FrameworkHandle<struct FrameGraphNodeHandleTag>;

	enum class FRAME_RESOURCE
	{
		BUFFER,
		IMAGE
	};

	struct GraphPreRenderInfo
	{
		void* instance;
		RENDER_IMAGE_LAYOUT startTransition;
		RENDER_IMAGE_LAYOUT endTransition;
	};

	struct GraphRenderInfo
	{
		void* instance;
	};

	struct GraphPostRenderInfo
	{
		void* instance;
		RENDER_IMAGE_LAYOUT startTransition;
		RENDER_IMAGE_LAYOUT endTransition;
	};

	typedef void (*PFN_GraphPreRender)(const CommandListHandle, const GraphPreRenderInfo&);
	typedef void (*PFN_GraphRender)(const CommandListHandle, const GraphRenderInfo&);
	typedef void (*PFN_GraphPostRender)(const CommandListHandle, const GraphPostRenderInfo&);

	struct FrameGraphRenderPass
	{
		void* instance;
		PFN_GraphPreRender preRenderFunc;
		PFN_GraphRender renderFunc;
		PFN_GraphPostRender postRenderFunc;
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
		};
	};
}