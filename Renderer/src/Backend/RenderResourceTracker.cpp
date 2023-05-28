#include "RenderResourceTracker.h"
#include "Editor.h"

using namespace BB;

void BB::RenderResourceTracker::AddDescriptor(const RenderDescriptorCreateInfo& a_Descriptor, const char* a_Name)
{
	AddEntry(a_Descriptor, RESOURCE_TYPE::DESCRIPTOR, a_Name);
}

void BB::RenderResourceTracker::AddQueue(const RenderCommandQueueCreateInfo& a_Queue, const char* a_Name)
{
	AddEntry(a_Queue, RESOURCE_TYPE::COMMAND_QUEUE, a_Name);
}

void BB::RenderResourceTracker::AddCommandAllocator(const RenderCommandAllocatorCreateInfo& a_CommandAllocator, const char* a_Name)
{
	AddEntry(a_CommandAllocator, RESOURCE_TYPE::COMMAND_ALLOCATOR, a_Name);
}

void BB::RenderResourceTracker::AddCommandList(const RenderCommandListCreateInfo& a_CommandList, const char* a_Name)
{
	AddEntry(a_CommandList, RESOURCE_TYPE::COMMAND_LIST, a_Name);
}

void BB::RenderResourceTracker::AddPipeline(const PipelineDebugInfo& a_Pipeline, const char* a_Name)
{
	//Pipeline has multiple dynamic entries so account for that.
	const size_t t_EntrySize = sizeof(PipelineDebugInfo) + sizeof(Entry);
	const size_t t_ShaderInfoSize = a_Pipeline.shaderCount * sizeof(PipelineDebugInfo::ShaderInfo);
	const size_t t_AttributeSize = a_Pipeline.attributeCount * sizeof(VertexAttributeDesc);

	Entry* t_Entry = reinterpret_cast<Entry*>(BBalloc(m_Allocator, t_EntrySize + t_ShaderInfoSize + t_AttributeSize));
	t_Entry->type = RESOURCE_TYPE::PIPELINE;
	t_Entry->name = a_Name;
	t_Entry->next = m_HeadEntry;
	t_Entry->typeInfo = Pointer::Add(t_Entry, sizeof(Entry));
	PipelineDebugInfo* t_PipelineInfo = reinterpret_cast<PipelineDebugInfo*>(t_Entry->typeInfo);
	*t_PipelineInfo = a_Pipeline;

	for (size_t i = 0; i < a_Pipeline.shaderCount; i++)
	{
		t_PipelineInfo->shaderInfo = reinterpret_cast<PipelineDebugInfo::ShaderInfo*>(Pointer::Add(t_Entry, t_EntrySize));
		t_PipelineInfo->shaderInfo[i] = a_Pipeline.shaderInfo[i];
	}
	for (size_t i = 0; i < a_Pipeline.attributeCount; i++)
	{
		t_PipelineInfo->attributes = reinterpret_cast<VertexAttributeDesc*>(Pointer::Add(t_Entry, t_EntrySize + t_ShaderInfoSize));
		t_PipelineInfo->attributes[i] = a_Pipeline.attributes[i];
	}
	++m_Entries;
	m_HeadEntry = t_Entry;
}

void BB::RenderResourceTracker::AddBuffer(const RenderBufferCreateInfo& a_Buffer, const char* a_Name)
{
	AddEntry(a_Buffer, RESOURCE_TYPE::BUFFER, a_Name);
}

void BB::RenderResourceTracker::AddImage(const RenderImageCreateInfo& a_Image, const char* a_Name)
{
	AddEntry(a_Image, RESOURCE_TYPE::IMAGE, a_Name);
}

void BB::RenderResourceTracker::AddSampler(const SamplerCreateInfo& a_Sampler, const char* a_Name)
{
	AddEntry(a_Sampler, RESOURCE_TYPE::SAMPLER, a_Name);
}

void BB::RenderResourceTracker::AddFence(const FenceCreateInfo& a_Fence, const char* a_Name)
{
	AddEntry(a_Fence, RESOURCE_TYPE::FENCE, a_Name);
}

void BB::RenderResourceTracker::Editor() const
{
	Editor::DisplayRenderResources(*this);
}