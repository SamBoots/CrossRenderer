#include "LightSystem.h"
#include "RenderBackend.h"
#include "Editor.h"

using namespace BB;

BB::LightPool::LightPool(UploadBuffer& a_UploadBuffer, LinearRenderBuffer& a_GPUBuffer, const uint32_t a_LightCount)
	:	m_LightMax(a_LightCount), m_UploadBuffer(a_UploadBuffer.Buffer())
{
	UploadBufferChunk t_UploadChunk = a_UploadBuffer.Alloc(sizeof(Light) * a_LightCount);
	m_UploadBufferOffset = static_cast<uint32_t>(t_UploadChunk.offset);

	m_LightsCPU = reinterpret_cast<Light*>(t_UploadChunk.memory);
	m_BufferPart = a_GPUBuffer.SubAllocate(static_cast<uint64_t>(a_LightCount * sizeof(Light)), 1);
	m_LightCount = 0;
}

BB::LightPool::~LightPool()
{

}

const LightHandle BB::LightPool::AddLight(const Light& a_Light)
{
	BB_ASSERT(m_LightMax > m_LightCount + 1, "Light pool gone over the amount of lights!");

	LightHandle t_Handle;
	t_Handle.index = m_LightCount;
	t_Handle.extraIndex = 1;

	m_LightsCPU[m_LightCount++] = a_Light;

	return t_Handle;
}

const LightHandle BB::LightPool::AddLights(const BB::Slice<Light> a_Lights)
{
	BB_ASSERT(m_LightMax > m_LightCount + a_Lights.size(), "Light pool gone over the amount of lights!");

	LightHandle t_Handle;
	t_Handle.index = m_LightCount;
	t_Handle.extraIndex = static_cast<uint32_t>(a_Lights.size());

	Memory::Copy(m_LightsCPU + m_LightCount, a_Lights.data(), a_Lights.size());

	m_LightCount += static_cast<uint32_t>(a_Lights.size());

	return t_Handle;
}

void BB::LightPool::SubmitLightsToGPU(const CommandListHandle t_RecordingCmdList, const BB::Slice<LightHandle> a_LightHandles) const
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

	RenderCopyBufferInfo t_CopyInfo{};
	t_CopyInfo.size = t_AllocSize;
	t_CopyInfo.src = m_UploadBuffer;
	t_CopyInfo.srcOffset = m_UploadBufferOffset;
	t_CopyInfo.dst = m_BufferPart.buffer;
	t_CopyInfo.dstOffset = static_cast<uint64_t>(m_BufferPart.offset + t_DstBufferOffset);

	RenderBackend::CopyBuffer(t_RecordingCmdList, t_CopyInfo);
}

void BB::LightPool::ResetLights()
{
	m_LightCount = 0;
}

LightSystem::LightSystem(const uint32_t a_LightAmount)
	:	m_UploadBuffer(sizeof(Light) * a_LightAmount, "light system transfer buffer"),
		m_LightGPUBuffer(RenderBufferCreateInfo{ "light system buffer", BB::mbSize * 2, RENDER_BUFFER_USAGE::STORAGE, RENDER_MEMORY_PROPERTIES::DEVICE_LOCAL}),
		m_Lights(m_UploadBuffer, m_LightGPUBuffer, a_LightAmount)
{
	
}

LightSystem::~LightSystem()
{}

LightHandle LightSystem::AddLights(const BB::Slice<Light> a_Lights, const LIGHT_TYPE a_LightType, const CommandListHandle a_CmdList)
{
	LightHandle t_Handle{};
	switch (a_LightType)
	{
	case LIGHT_TYPE::POINT:
		t_Handle = m_Lights.AddLights(a_Lights);
		m_Lights.SubmitLightsToGPU(a_CmdList, BB::Slice(&t_Handle, 1));
		break;
	}
	return t_Handle;
}

void LightSystem::UpdateDescriptor(const RDescriptor a_Descriptor, const DescriptorAllocation& a_Allocation)
{
	RenderBufferPart t_LightBufferPart = m_Lights.GetBufferAllocInfo();

	WriteDescriptorInfos t_WriteInfo;
	WriteDescriptorData t_WriteData{};
	t_WriteInfo.data = Slice(&t_WriteData, 1);
	t_WriteInfo.allocation = a_Allocation;
	t_WriteInfo.descriptorHandle = a_Descriptor;

	t_WriteData.binding = 3;
	t_WriteData.descriptorIndex = 0;
	t_WriteData.type = RENDER_DESCRIPTOR_TYPE::READONLY_BUFFER;
	t_WriteData.buffer.buffer = t_LightBufferPart.buffer;
	t_WriteData.buffer.offset = t_LightBufferPart.offset;
	t_WriteData.buffer.range = t_LightBufferPart.size;

	RenderBackend::WriteDescriptors(t_WriteInfo);
}