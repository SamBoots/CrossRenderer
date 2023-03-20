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

void BB::LightPool::SubmitLights(const RecordingCommandListHandle t_RecordingCmdList, UploadBuffer& a_UploadBuffer, const Light* a_Lights, const uint32_t a_Count)
{
	BB_ASSERT(m_LightCount + a_Count > m_LightMax, "Light pool gone over the amount of lights!");

	const uint32_t t_AllocSize = sizeof(Light) * a_Count;
	const uint32_t t_DstBufferOffset = sizeof(Light) * m_LightCount;

	//Add the lights here.
	UploadBufferChunk t_UploadChunk = a_UploadBuffer.Alloc(t_AllocSize);

	Memory::Copy(reinterpret_cast<Light*>(t_UploadChunk.memory), a_Lights, a_Count);

	RenderCopyBufferInfo t_CopyInfo{};
	t_CopyInfo.dst = m_BufferPart.bufferHandle;
	t_CopyInfo.dstOffset = static_cast<uint64_t>(m_BufferPart.offset + t_DstBufferOffset);
	t_CopyInfo.src = a_UploadBuffer.Buffer();
	t_CopyInfo.srcOffset = t_UploadChunk.offset;
	t_CopyInfo.size = t_AllocSize;

	RenderBackend::CopyBuffer(t_RecordingCmdList, t_CopyInfo);

	m_LightCount += a_Count;
}

void BB::LightPool::ResetLights()
{
	m_LightCount = 0;
}