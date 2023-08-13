#pragma once
#include "FrameGraphCommon.hpp"

namespace BB
{
	class FrameGraph
	{
	public:
		FrameGraph();
		~FrameGraph();

		//TEMP, before builder
		void RegisterRenderPass(FrameGraphRenderPass a_RenderPass);

		//temporary for now.
		void BeginRendering();
		void Render();
		void EndRendering();

		const FrameGraphResourceHandle CreateResource(const FrameGraphResource& a_Resource);
		void DestroyResource(const FrameGraphResourceHandle a_Handle);

	private:
		FreelistAllocator_t m_Allocator{ mbSize * 32, "Framegraph allocator"};
		struct FrameGraph_inst* inst;
	};
}