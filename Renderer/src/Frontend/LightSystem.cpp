#include "LightSystem.h"
#include "RenderBackend.h"

using namespace BB;

BB::LightPool::LightPool(Allocator a_SystemAllocator, LinearRenderBuffer& a_GPUBuffer, const uint32_t a_LightCount)
	: m_LightMax(a_LightCount)
{
	m_LightsCPU = BBnewArr(a_SystemAllocator, a_LightCount, Light);
	m_BufferPart = a_GPUBuffer.SubAllocateFromBuffer(static_cast<uint64_t>(a_LightCount * sizeof(Light)), 1);
	m_LightCount = 0;
}

BB::LightPool::~LightPool()
{

}

const LightHandle BB::LightPool::AddLight(Light& a_Light)
{
	BB_ASSERT(m_LightMax > m_LightCount + 1, "Light pool gone over the amount of lights!");

	LightHandle t_Handle;
	t_Handle.index = m_LightCount;
	t_Handle.extraIndex = 1;

	Light& t_CopyAddress = m_LightsCPU[m_LightCount];
	t_CopyAddress = a_Light;

	++m_LightCount;
	return t_Handle;
}

const LightHandle BB::LightPool::AddLights(const BB::Slice<Light> a_Lights)
{
	BB_ASSERT(m_LightMax > m_LightCount + a_Lights.size(), "Light pool gone over the amount of lights!");

	LightHandle t_Handle;
	t_Handle.index = m_LightCount;
	t_Handle.extraIndex = a_Lights.size();

	Memory::Copy(m_LightsCPU + m_LightCount, a_Lights.data(), a_Lights.size());

	m_LightCount += a_Lights.size();

	return t_Handle;
}

void BB::LightPool::SubmitLightsToGPU(const RecordingCommandListHandle t_RecordingCmdList, UploadBuffer& a_UploadBuffer, const BB::Slice<LightHandle> a_LightHandles) const
{
	BB_ASSERT(m_LightCount > 0, "Light pool is empty!");
	uint32_t t_FirstIndex = 0;
	uint32_t t_LastIndex = 0;

	//If we supply no light handles we just upload it all.
	if (a_LightHandles.size() == 0)
		t_LastIndex = m_LightCount;
	else
		t_FirstIndex = a_LightHandles[0].index;

	for (size_t i = 0; i < a_LightHandles.size(); i++)
	{
		//Get the end index of the regions we want to copy over.
		const uint32_t t_EndIndex = a_LightHandles[i].index + a_LightHandles[i].extraIndex;
		if (t_EndIndex > t_LastIndex)
			t_LastIndex = t_EndIndex;

		//Get the first index of the regions we want to copy over.
		if (t_FirstIndex > a_LightHandles[i].extraIndex)
			t_FirstIndex = a_LightHandles[i].extraIndex;
	}



	const uint32_t t_LightAmount = t_LastIndex - t_FirstIndex;

	const uint64_t t_AllocSize = static_cast<uint64_t>(t_LightAmount * sizeof(Light));
	const uint64_t t_DstBufferOffset = static_cast<uint64_t>(t_FirstIndex * sizeof(Light));

	//Add the lights here.
	UploadBufferChunk t_UploadChunk = a_UploadBuffer.Alloc(t_AllocSize);

	Memory::Copy(
		reinterpret_cast<Light*>(t_UploadChunk.memory), 
		&m_LightsCPU[t_FirstIndex], 
		t_LightAmount);

	RenderCopyBufferInfo t_CopyInfo{};
	t_CopyInfo.dst = m_BufferPart.bufferHandle;
	t_CopyInfo.dstOffset = static_cast<uint64_t>(m_BufferPart.offset + t_DstBufferOffset);
	t_CopyInfo.src = a_UploadBuffer.Buffer();
	t_CopyInfo.srcOffset = t_UploadChunk.offset;
	t_CopyInfo.size = t_AllocSize;

	RenderBackend::CopyBuffer(t_RecordingCmdList, t_CopyInfo);
}

void BB::LightPool::ResetLights()
{
	m_LightCount = 0;
}