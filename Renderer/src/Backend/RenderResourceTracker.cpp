#include "RenderResourceTracker.h"
#include "Editor.h"

using namespace BB;

void BB::RenderResourceTracker::AddDescriptor(const RenderDescriptorCreateInfo& a_Descriptor, const char* a_Name, const uint64_t a_ID)
{
	AddEntry(a_Descriptor, RESOURCE_TYPE::DESCRIPTOR, a_Name, a_ID);
}

void BB::RenderResourceTracker::AddQueue(const RenderCommandQueueCreateInfo& a_Queue, const char* a_Name, const uint64_t a_ID)
{
	AddEntry(a_Queue, RESOURCE_TYPE::COMMAND_QUEUE, a_Name, a_ID);
}

void BB::RenderResourceTracker::AddCommandAllocator(const RenderCommandAllocatorCreateInfo& a_CommandAllocator, const char* a_Name, const uint64_t a_ID)
{
	AddEntry(a_CommandAllocator, RESOURCE_TYPE::COMMAND_ALLOCATOR, a_Name, a_ID);
}

void BB::RenderResourceTracker::AddCommandList(const RenderCommandListCreateInfo& a_CommandList, const char* a_Name, const uint64_t a_ID)
{
	AddEntry(a_CommandList, RESOURCE_TYPE::COMMAND_LIST, a_Name, a_ID);
}

void BB::RenderResourceTracker::AddPipeline(const PipelineDebugInfo& a_Pipeline, const char* a_Name, const uint64_t a_ID)
{
	//Pipeline has multiple dynamic entries so account for that.
	const size_t t_EntrySize = sizeof(PipelineDebugInfo) + sizeof(Entry);
	const size_t t_ShaderInfoSize = a_Pipeline.shaderCount * sizeof(PipelineDebugInfo::ShaderInfo);
	const size_t t_AttributeSize = a_Pipeline.attributeCount * sizeof(VertexAttributeDesc);

	Entry* t_Entry = reinterpret_cast<Entry*>(BBalloc(m_Allocator, t_EntrySize + t_ShaderInfoSize + t_AttributeSize));
	t_Entry->type = RESOURCE_TYPE::PIPELINE;
	t_Entry->id = a_ID;
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

void BB::RenderResourceTracker::AddBuffer(const RenderBufferCreateInfo& a_Buffer, const char* a_Name, const uint64_t a_ID)
{
	AddEntry(a_Buffer, RESOURCE_TYPE::BUFFER, a_Name, a_ID);
}

void BB::RenderResourceTracker::AddImage(const RenderImageCreateInfo& a_Image, const char* a_Name, const uint64_t a_ID)
{
	AddEntry(a_Image, RESOURCE_TYPE::IMAGE, a_Name, a_ID);
}

void BB::RenderResourceTracker::AddSampler(const SamplerCreateInfo& a_Sampler, const char* a_Name, const uint64_t a_ID)
{
	AddEntry(a_Sampler, RESOURCE_TYPE::SAMPLER, a_Name, a_ID);
}

void BB::RenderResourceTracker::AddFence(const FenceCreateInfo& a_Fence, const char* a_Name, const uint64_t a_ID)
{
	AddEntry(a_Fence, RESOURCE_TYPE::FENCE, a_Name, a_ID);
}

void BB::RenderResourceTracker::Editor()
{
	Editor::DisplayRenderResources(*this);
}

static RenderResourceTracker::Entry* GetMiddleList(RenderResourceTracker::Entry* a_Head)
{
	//Slow is the middle of the list after the iteration.
	RenderResourceTracker::Entry* t_Slow = a_Head;
	RenderResourceTracker::Entry* t_Fast = a_Head->next;

	while (t_Fast != nullptr)
	{
		t_Fast = t_Fast->next;
		if (t_Fast != nullptr)
		{
			t_Slow = t_Slow->next;
			t_Fast = t_Fast->next;
		}
	}

	RenderResourceTracker::Entry* t_ReturnValue = t_Slow->next;
	//Seperate the front away from the middle.
	t_Slow->next = NULL;
	return t_ReturnValue;
}

RenderResourceTracker::Entry* SortedTypeMerge(RenderResourceTracker::Entry* a_First, RenderResourceTracker::Entry* a_Second)
{
	RenderResourceTracker::Entry* t_ReturnValue = nullptr;

	if (a_First == nullptr)
		return a_Second;
	else if (a_Second == nullptr)
		return (a_First);

	if (static_cast<uint32_t>(a_First->type) <= static_cast<uint32_t>(a_Second->type))
	{
		t_ReturnValue = a_First;
		t_ReturnValue->next = SortedTypeMerge(a_First->next, a_Second);
	}
	else
	{
		t_ReturnValue = a_Second;
		t_ReturnValue->next = SortedTypeMerge(a_First, a_Second->next);
	}
	return t_ReturnValue;
}

static void SortEntriesByType(RenderResourceTracker::Entry** a_Head)
{
	RenderResourceTracker::Entry* t_Head = *a_Head;

	if (t_Head == nullptr || t_Head->next == nullptr)
		return;

	RenderResourceTracker::Entry* t_A = t_Head;
	RenderResourceTracker::Entry* t_B = GetMiddleList(t_Head);

	SortEntriesByType(&t_A);
	SortEntriesByType(&t_B);

	*a_Head = SortedTypeMerge(t_A, t_B);
}

void BB::RenderResourceTracker::SortByType()
{
	m_SortType = SORT_TYPE::TYPE;
	SortEntriesByType(&m_HeadEntry);
}

RenderResourceTracker::Entry* SortedTimeMerge(RenderResourceTracker::Entry* a_First, RenderResourceTracker::Entry* a_Second)
{
	RenderResourceTracker::Entry* t_ReturnValue = nullptr;

	if (a_First == nullptr)
		return a_Second;
	else if (a_Second == nullptr)
		return (a_First);

	if (a_First->timeId > a_Second->timeId)
	{
		t_ReturnValue = a_First;
		t_ReturnValue->next = SortedTimeMerge(a_First->next, a_Second);
	}
	else
	{
		t_ReturnValue = a_Second;
		t_ReturnValue->next = SortedTimeMerge(a_First, a_Second->next);
	}
	return t_ReturnValue;
}

static void SortEntriesByTime(RenderResourceTracker::Entry** a_Head)
{
	RenderResourceTracker::Entry* t_Head = *a_Head;

	if (t_Head == nullptr || t_Head->next == nullptr)
		return;

	RenderResourceTracker::Entry* t_A = t_Head;
	RenderResourceTracker::Entry* t_B = GetMiddleList(t_Head);

	SortEntriesByTime(&t_A);
	SortEntriesByTime(&t_B);

	*a_Head = SortedTimeMerge(t_A, t_B);
}

void BB::RenderResourceTracker::SortByTime()
{
	m_SortType = SORT_TYPE::TIME;
	SortEntriesByTime(&m_HeadEntry);
}