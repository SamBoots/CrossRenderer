#include "LightSystem.h"
#include "RenderBackend.h"

using namespace BB;

BB::LightPool::LightPool(LinearRenderBuffer& a_GPUBuffer, const uint32_t a_LightCount)
	: m_LightMax(a_LightCount)
{
	m_BufferPart = a_GPUBuffer.SubAllocateFromBuffer(static_cast<uint64_t>(a_LightCount * sizeof(Light)), 1);
	m_LightCount = 0;
}

BB::LightPool::~LightPool()
{

}

void BB::LightPool::SubmitLights(const RecordingCommandListHandle t_RecordingCmdList, UploadBuffer& a_UploadBuffer, const BB::Slice<Light> a_Lights)
{
	BB_ASSERT(m_LightMax > m_LightCount + a_Lights.size(), "Light pool gone over the amount of lights!");

	const uint64_t t_AllocSize = static_cast<uint64_t>(a_Lights.size() * sizeof(Light));
	const uint64_t t_DstBufferOffset = static_cast<uint64_t>(m_LightCount * sizeof(Light));

	//Add the lights here.
	UploadBufferChunk t_UploadChunk = a_UploadBuffer.Alloc(t_AllocSize);

	Memory::Copy(reinterpret_cast<Light*>(t_UploadChunk.memory), a_Lights.data(), a_Lights.size());

	RenderCopyBufferInfo t_CopyInfo{};
	t_CopyInfo.dst = m_BufferPart.bufferHandle;
	t_CopyInfo.dstOffset = static_cast<uint64_t>(m_BufferPart.offset + t_DstBufferOffset);
	t_CopyInfo.src = a_UploadBuffer.Buffer();
	t_CopyInfo.srcOffset = t_UploadChunk.offset;
	t_CopyInfo.size = t_AllocSize;

	RenderBackend::CopyBuffer(t_RecordingCmdList, t_CopyInfo);

	m_LightCount += static_cast<uint32_t>(a_Lights.size());
}

void BB::LightPool::ResetLights()
{
	m_LightCount = 0;
}