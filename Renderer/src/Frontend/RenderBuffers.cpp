#include "RenderBuffers.h"

using namespace BB;

LinearRenderBuffer::LinearRenderBuffer(const RenderBufferCreateInfo& a_CreateInfo)
	: m_Size(a_CreateInfo.size), m_MemoryProperties(a_CreateInfo.memProperties)
{
	m_Buffer = RenderBackend::CreateBuffer(a_CreateInfo);
	m_Used = 0;
}

LinearRenderBuffer::~LinearRenderBuffer()
{
	RenderBackend::DestroyBuffer(m_Buffer);
}

RenderBufferPart BB::LinearRenderBuffer::SubAllocateFromBuffer(const uint64_t a_Size, const uint32_t a_Alignment)
{
	//Align the m_Used variable, as it works as the buffer offset.
	m_Used += static_cast<uint32_t>(Pointer::AlignForwardAdjustment(a_Size, a_Alignment));
	BB_ASSERT(m_Size >= static_cast<uint64_t>(m_Used + a_Size), "Not enough memory for a linear render buffer!");

	RenderBufferPart t_Part{};
	t_Part.bufferHandle = m_Buffer;
	t_Part.size = static_cast<uint32_t>(a_Size);
	t_Part.offset = static_cast<uint32_t>(m_Used);

	m_Used += a_Size;

	return t_Part;
}

void LinearRenderBuffer::MapBuffer() const
{
	BB_ASSERT(m_MemoryProperties == RENDER_MEMORY_PROPERTIES::HOST_VISIBLE,
		"Trying to map a GPU device local memory region! Create the buffer with HOST_VISIBLE instead!");
	RenderBackend::MapMemory(m_Buffer);
}

void LinearRenderBuffer::UnmapBuffer() const
{
	BB_ASSERT(m_MemoryProperties == RENDER_MEMORY_PROPERTIES::HOST_VISIBLE,
		"Trying to unmap a GPU device local memory region! Create the buffer with HOST_VISIBLE instead!");
	RenderBackend::UnmapMemory(m_Buffer);
}