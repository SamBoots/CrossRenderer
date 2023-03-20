#pragma once
#include "RenderFrontendCommon.h"

namespace BB
{
	class LinearRenderBuffer
	{
	public:
		LinearRenderBuffer(const RenderBufferCreateInfo& a_CreateInfo);
		~LinearRenderBuffer();

		//Maybe do alignment
		RenderBufferPart SubAllocateFromBuffer(const uint64_t a_Size, const uint32_t a_Alignment);

		void MapBuffer() const;
		void UnmapBuffer() const;

	private:
		const RENDER_MEMORY_PROPERTIES m_MemoryProperties;
		RBufferHandle m_Buffer;
		const uint64_t m_Size;
		uint64_t m_Used;
	};
}