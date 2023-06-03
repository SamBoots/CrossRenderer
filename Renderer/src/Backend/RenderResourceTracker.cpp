#include "RenderResourceTracker.h"
#include "Hashmap.h"
#include "Editor.h"

using namespace BB;

enum class SORT_TYPE : uint32_t
{
	TIME,
	TYPE
};

struct Entry
{
	RESOURCE_TYPE type{};
	uint64_t timeId = 0;
	uint64_t id = NULL;
	const char* name = nullptr;
	Entry* next = nullptr;
	void* typeInfo = nullptr;
};

class BB::RenderResourceTracker_Inst
{
public:
	RenderResourceTracker_Inst(Allocator a_Allocator) : entryMap(a_Allocator, 128) {};

	OL_HashMap<uint64_t, Entry*> entryMap;
	SORT_TYPE sortType = SORT_TYPE::TIME;
	uint64_t timeID = 0;
	uint32_t entries = 0;
	Entry* headEntry = nullptr;


	template<typename T>
	void AddEntry(Allocator a_Allocator, const T& a_TypeInfo, const RESOURCE_TYPE a_Type, const char* a_Name, const uint64_t a_ID)
	{
		const size_t t_EntrySize = sizeof(T) + sizeof(Entry);
		Entry* t_Entry = reinterpret_cast<Entry*>(BBalloc(a_Allocator, t_EntrySize));
		t_Entry->type = a_Type;
		t_Entry->timeId = timeID++;
		t_Entry->id = a_ID;
		t_Entry->name = a_Name;
		t_Entry->typeInfo = Pointer::Add(t_Entry, sizeof(Entry));
		T* t_Obj = reinterpret_cast<T*>(t_Entry->typeInfo);
		*t_Obj = a_TypeInfo;
		++entries;
		switch (sortType)
		{
		case SORT_TYPE::TIME:
			t_Entry->next = headEntry;
			headEntry = t_Entry;
			break;
		case SORT_TYPE::TYPE:
		{
			Entry* t_SearchEntry = headEntry;
			while (t_SearchEntry != nullptr)
			{
				if (static_cast<uint32_t>(t_SearchEntry->next->type) >= static_cast<uint32_t>(t_Entry->type))
				{
					t_Entry->next = t_SearchEntry->next;
					t_SearchEntry->next = t_Entry;
					return;
				}

				t_SearchEntry = t_SearchEntry->next;
			}
			//None found, maybe first in the entry? Set it to the top
			t_SearchEntry->next = t_Entry;
			t_Entry->next = nullptr;
		}
		break;
		default:
			break;
		}

		entryMap.insert(t_Entry->id, t_Entry);
	}
};


BB::RenderResourceTracker::RenderResourceTracker()
{
	m_Instance = BBnew(m_Allocator, RenderResourceTracker_Inst)(m_Allocator);
}

BB::RenderResourceTracker::~RenderResourceTracker()
{

	BBfree(m_Allocator, m_Instance);
}

void BB::RenderResourceTracker::AddDescriptor(const RenderDescriptorCreateInfo& a_Descriptor, const char* a_Name, const uint64_t a_ID)
{
	m_Instance->AddEntry(m_Allocator, a_Descriptor, RESOURCE_TYPE::DESCRIPTOR, a_Name, a_ID);
}

void BB::RenderResourceTracker::AddQueue(const RenderCommandQueueCreateInfo& a_Queue, const char* a_Name, const uint64_t a_ID)
{
	m_Instance->AddEntry(m_Allocator, a_Queue, RESOURCE_TYPE::COMMAND_QUEUE, a_Name, a_ID);
}

void BB::RenderResourceTracker::AddCommandAllocator(const RenderCommandAllocatorCreateInfo& a_CommandAllocator, const char* a_Name, const uint64_t a_ID)
{
	m_Instance->AddEntry(m_Allocator, a_CommandAllocator, RESOURCE_TYPE::COMMAND_ALLOCATOR, a_Name, a_ID);
}

void BB::RenderResourceTracker::AddCommandList(const RenderCommandListCreateInfo& a_CommandList, const char* a_Name, const uint64_t a_ID)
{
	m_Instance->AddEntry(m_Allocator, a_CommandList, RESOURCE_TYPE::COMMAND_LIST, a_Name, a_ID);
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
	t_Entry->next = m_Instance->headEntry;
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
	++m_Instance->entries;
	m_Instance->headEntry = t_Entry;

	m_Instance->entryMap.insert(t_Entry->id, t_Entry);
}

void BB::RenderResourceTracker::AddBuffer(const RenderBufferCreateInfo& a_Buffer, const char* a_Name, const uint64_t a_ID)
{
	m_Instance->AddEntry(m_Allocator, a_Buffer, RESOURCE_TYPE::BUFFER, a_Name, a_ID);
}

void BB::RenderResourceTracker::AddImage(const RenderImageCreateInfo& a_Image, const char* a_Name, const uint64_t a_ID)
{
	m_Instance->AddEntry(m_Allocator, a_Image, RESOURCE_TYPE::IMAGE, a_Name, a_ID);
}

void BB::RenderResourceTracker::AddSampler(const SamplerCreateInfo& a_Sampler, const char* a_Name, const uint64_t a_ID)
{
	m_Instance->AddEntry(m_Allocator, a_Sampler, RESOURCE_TYPE::SAMPLER, a_Name, a_ID);
}

void BB::RenderResourceTracker::AddFence(const FenceCreateInfo& a_Fence, const char* a_Name, const uint64_t a_ID)
{
	m_Instance->AddEntry(m_Allocator, a_Fence, RESOURCE_TYPE::FENCE, a_Name, a_ID);
}

void BB::RenderResourceTracker::RemoveEntry(const uint64_t a_ID)
{
	Entry* t_PreviousEntry = nullptr;
	Entry* t_Entry = m_Instance->headEntry;
	for (size_t i = 0; i < m_Instance->entries; i++)
	{
		BB_ASSERT(t_Entry != nullptr, "Trying to remove an entry but iterating over too many elements!");
		if (t_Entry->id == a_ID)
		{
			if (t_PreviousEntry == nullptr)
			{
				m_Instance->headEntry = t_Entry->next;
				//memset the header to 0 for safety, the typeInfo is not null.
				memset(t_Entry, 0, sizeof(Entry));
				BBfree(m_Allocator, t_Entry);
				return;
			}
			t_PreviousEntry->next = t_Entry->next;
			memset(t_Entry, 0, sizeof(Entry));
			BBfree(m_Allocator, t_Entry);
			return;
		}
		t_PreviousEntry = t_Entry;
		t_Entry = t_Entry->next;
	}
	--m_Instance->entries;
	m_Instance->entryMap.erase(t_Entry->id);
}

void BB::RenderResourceTracker::Editor()
{
	Editor::DisplayRenderResources(*this);
}

static Entry* GetMiddleList(Entry* a_Head)
{
	//Slow is the middle of the list after the iteration.
	Entry* t_Slow = a_Head;
	Entry* t_Fast = a_Head->next;

	while (t_Fast != nullptr)
	{
		t_Fast = t_Fast->next;
		if (t_Fast != nullptr)
		{
			t_Slow = t_Slow->next;
			t_Fast = t_Fast->next;
		}
	}

	Entry* t_ReturnValue = t_Slow->next;
	//Seperate the front away from the middle.
	t_Slow->next = NULL;
	return t_ReturnValue;
}

Entry* SortedTypeMerge(Entry* a_First, Entry* a_Second)
{
	Entry* t_ReturnValue = nullptr;

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

static void SortEntriesByType(Entry** a_Head)
{
	Entry* t_Head = *a_Head;

	if (t_Head == nullptr || t_Head->next == nullptr)
		return;

	Entry* t_A = t_Head;
	Entry* t_B = GetMiddleList(t_Head);

	SortEntriesByType(&t_A);
	SortEntriesByType(&t_B);

	*a_Head = SortedTypeMerge(t_A, t_B);
}

void BB::RenderResourceTracker::SortByType()
{
	m_Instance->sortType = SORT_TYPE::TYPE;
	SortEntriesByType(&m_Instance->headEntry);
}

Entry* SortedTimeMerge(Entry* a_First, Entry* a_Second)
{
	Entry* t_ReturnValue = nullptr;

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

static void SortEntriesByTime(Entry** a_Head)
{
	Entry* t_Head = *a_Head;

	if (t_Head == nullptr || t_Head->next == nullptr)
		return;

	Entry* t_A = t_Head;
	Entry* t_B = GetMiddleList(t_Head);

	SortEntriesByTime(&t_A);
	SortEntriesByTime(&t_B);

	*a_Head = SortedTimeMerge(t_A, t_B);
}

void BB::RenderResourceTracker::SortByTime()
{
	m_Instance->sortType = SORT_TYPE::TIME;
	SortEntriesByTime(&m_Instance->headEntry);
}

#pragma region Editor
static inline const char* DescriptorTypeStr(const RENDER_DESCRIPTOR_TYPE a_Type)
{
	switch (a_Type)
	{
	case RENDER_DESCRIPTOR_TYPE::READONLY_CONSTANT:			return "Descriptor Type: READONLY_CONSTANT";
	case RENDER_DESCRIPTOR_TYPE::READONLY_BUFFER:			return "Descriptor Type: READONLY_BUFFER";
	case RENDER_DESCRIPTOR_TYPE::READWRITE:					return "Descriptor Type: READWRITE";
	case RENDER_DESCRIPTOR_TYPE::READONLY_CONSTANT_DYNAMIC:	return "Descriptor Type: READONLY_CONSTANT_DYNAMIC";
	case RENDER_DESCRIPTOR_TYPE::READONLY_BUFFER_DYNAMIC:	return "Descriptor Type: READONLY_BUFFER_DYNAMIC";
	case RENDER_DESCRIPTOR_TYPE::READWRITE_DYNAMIC:			return "Descriptor Type: READWRITE_DYNAMIC";
	case RENDER_DESCRIPTOR_TYPE::IMAGE:						return "Descriptor Type: IMAGE";
	case RENDER_DESCRIPTOR_TYPE::SAMPLER:					return "Descriptor Type: SAMPLER";
	default:
		break;
	}
}

static inline const char* ShaderStageStr(const RENDER_SHADER_STAGE a_Stage)
{
	switch (a_Stage)
	{
	case RENDER_SHADER_STAGE::ALL:				return "Shader Stage: ALL";
	case RENDER_SHADER_STAGE::VERTEX:			return "Shader Stage: VERTEX";
	case RENDER_SHADER_STAGE::FRAGMENT_PIXEL:	return "Shader Stage: FRAGMENT_PIXEL";
	default:
		BB_ASSERT(false, "RENDER_SHADER_STAGE unknown in resource tracker!");
		break;
	}
}

static inline const char* DescriptorFlagStr(const RENDER_DESCRIPTOR_FLAG a_Flag)
{
	switch (a_Flag)
	{
	case RENDER_DESCRIPTOR_FLAG::NONE:		return "Flags: NONE";
	case RENDER_DESCRIPTOR_FLAG::BINDLESS:	return "Flags: BINDLESS";
	default:
		BB_ASSERT(false, "RENDER_DESCRIPTOR_FLAG unknown in resource tracker!");
		break;
	}
}

static inline const char* InputFormat(const RENDER_INPUT_FORMAT a_Format)
{
	switch (a_Format)
	{
	case RENDER_INPUT_FORMAT::RGBA32:	return "INPUT FORMAT: RGBA32";
	case RENDER_INPUT_FORMAT::RGB32:	return "INPUT FORMAT: RGB32";
	case RENDER_INPUT_FORMAT::RG32:		return "INPUT FORMAT: RG32";
	case RENDER_INPUT_FORMAT::R32:		return "INPUT FORMAT: R32";
	case RENDER_INPUT_FORMAT::RGBA8:	return "INPUT FORMAT: RGBA8";
	case RENDER_INPUT_FORMAT::RG8:		return "INPUT FORMAT: RG8";

	default:
		BB_ASSERT(false, "RENDER_INPUT_FORMAT unknown in resource tracker!");
		break;
	}
}

static inline const char* BlendFactor(const RENDER_BLEND_FACTOR a_BlendFac)
{
	switch (a_BlendFac)
	{
	case RENDER_BLEND_FACTOR::ZERO:					return "ZERO";
	case RENDER_BLEND_FACTOR::ONE:					return "ONE";
	case RENDER_BLEND_FACTOR::SRC_ALPHA:			return "SRC_ALPHA";
	case RENDER_BLEND_FACTOR::ONE_MINUS_SRC_ALPHA:	return "ONE_MINUS_SRC_ALPHA";

	default:
		BB_ASSERT(false, "RENDER_BLEND_FACTOR unknown in resource tracker!");
		break;
	}
}

static inline const char* BlendOp(const RENDER_BLEND_OP a_BlendOp)
{
	switch (a_BlendOp)
	{
	case RENDER_BLEND_OP::ADD:		return "Blend Op: ADD";
	case RENDER_BLEND_OP::SUBTRACT:	return "Blend Op: SUBTRACT";

	default:
		BB_ASSERT(false, "RENDER_BLEND_OP unknown in resource tracker!");
		break;
	}
}

#include "imgui.h"
void BB::Editor::DisplayRenderResources(BB::RenderResourceTracker& a_ResTracker)
{
	RenderResourceTracker_Inst* t_Inst = a_ResTracker.m_Instance;

	if (ImGui::Begin("Render resources"))
	{
		if (ImGui::BeginMenu("Options"))
		{
			if (ImGui::Button("Sort by time"))
			{
				a_ResTracker.SortByTime();
			}
			if (ImGui::Button("Sort by type"))
			{
				a_ResTracker.SortByType();
			}
			ImGui::EndMenu();
		}
		Entry* t_Entry = t_Inst->headEntry;
		uint32_t t_EntryCount = 0;
		while (t_Entry != nullptr)
		{
			ImGui::PushID(t_EntryCount++);
			BB_ASSERT(t_EntryCount <= t_Inst->entries, "Render Resource tracker has too many entries while they are not marked!");
			const char* t_ResName = "UNNAMED";
			if (t_Entry->name != nullptr)
				t_ResName = t_Entry->name;

			if (ImGui::CollapsingHeader(t_ResName))
			{
				switch (t_Entry->type)
				{
				case RESOURCE_TYPE::DESCRIPTOR:
				{
					const RenderDescriptorCreateInfo& t_Desc =
						*reinterpret_cast<RenderDescriptorCreateInfo*>(t_Entry->typeInfo);
					switch (t_Desc.bindingSet)
					{
					case RENDER_BINDING_SET::PER_FRAME:
						ImGui::Text("Binding set: PER_FRAME");
						break;
					case RENDER_BINDING_SET::PER_PASS:
						ImGui::Text("Binding set: PER_PASS");
						break;
					case RENDER_BINDING_SET::PER_MATERIAL:
						ImGui::Text("Binding set: PER_MATERIAL");
						break;
					case RENDER_BINDING_SET::PER_OBJECT:
						ImGui::Text("Binding set: PER_OBJECT");
						break;
					default:
						BB_ASSERT(false, "Unknown RENDER_BINDING_SET for resource trackering editor");
						break;
					}

					for (size_t i = 0; i < t_Desc.bindings.size(); i++)
					{
						if (ImGui::TreeNode("PLACE_HOLDER_DESC_NAME_WILL_CRASH"))
						{
							const DescriptorBinding& t_Bind = t_Desc.bindings[i];
							ImGui::Text("Binding: %u", t_Bind.binding);
							ImGui::Text("DescriptorCount: %u", t_Bind.descriptorCount);
							ImGui::Text(DescriptorTypeStr(t_Bind.type));
							ImGui::Text(ShaderStageStr(t_Bind.stage));
							ImGui::Text(DescriptorFlagStr(t_Bind.flags));
							ImGui::TreePop();
						}
					}
				}
				break;
				case RESOURCE_TYPE::COMMAND_QUEUE:
				{
					const RenderCommandQueueCreateInfo& t_Queue =
						*reinterpret_cast<RenderCommandQueueCreateInfo*>(t_Entry->typeInfo);
					switch (t_Queue.queue)
					{
					case RENDER_QUEUE_TYPE::GRAPHICS:
						ImGui::Text("Queue type: GRAPHICS");
						break;
					case RENDER_QUEUE_TYPE::TRANSFER_COPY:
						ImGui::Text("Queue type: TRANSFER_COPY");
						break;
					case RENDER_QUEUE_TYPE::COMPUTE:
						ImGui::Text("Queue type: COMPUTE");
						break;
					default:
						BB_ASSERT(false, "Unknown RENDER_QUEUE_TYPE for resource trackering editor");
						break;
					}
					break;
				}
				case RESOURCE_TYPE::COMMAND_ALLOCATOR:
				{
					const RenderCommandAllocatorCreateInfo& t_Alloc =
						*reinterpret_cast<RenderCommandAllocatorCreateInfo*>(t_Entry->typeInfo);
					ImGui::Text("Commandlist count: %u", t_Alloc.commandListCount);
					switch (t_Alloc.queueType)
					{
					case RENDER_QUEUE_TYPE::GRAPHICS:
						ImGui::Text("Queue type: GRAPHICS");
						break;
					case RENDER_QUEUE_TYPE::TRANSFER_COPY:
						ImGui::Text("Queue type: TRANSFER_COPY");
						break;
					case RENDER_QUEUE_TYPE::COMPUTE:
						ImGui::Text("Queue type: COMPUTE");
						break;
					default:
						BB_ASSERT(false, "Unknown RENDER_QUEUE_TYPE for resource trackering editor");
						break;
					}
				}
				break;
				case RESOURCE_TYPE::COMMAND_LIST:
				{
					const RenderCommandListCreateInfo& t_List =
						*reinterpret_cast<RenderCommandListCreateInfo*>(t_Entry->typeInfo);
				}
				break;
				case RESOURCE_TYPE::PIPELINE:
				{
					const PipelineDebugInfo& t_Pipeline =
						*reinterpret_cast<PipelineDebugInfo*>(t_Entry->typeInfo);

					ImGui::Text("Depthtest enabled: %d", t_Pipeline.enableDepthTest);

					if (t_Pipeline.constantData.dwordSize)
						if (ImGui::TreeNode("Constant data"))
						{
							ImGui::Text("dwordsize: %u", t_Pipeline.constantData.dwordSize);
							ImGui::Text(ShaderStageStr(t_Pipeline.constantData.shaderStage));
							ImGui::TreePop();
						}

					if (ImGui::TreeNode("Rasterizer State"))
					{
						ImGui::Text("Counterclockwise: %d", t_Pipeline.rasterState.frontCounterClockwise);
						switch (t_Pipeline.rasterState.cullMode)
						{
						case RENDER_CULL_MODE::NONE:
							ImGui::Text("Cullmode: NONE");
							break;
						case RENDER_CULL_MODE::FRONT:
							ImGui::Text("Cullmode: FRONT");
							break;
						case RENDER_CULL_MODE::BACK:
							ImGui::Text("Cullmode: BACK");
							break;
						}
						ImGui::TreePop();
					}

					if (ImGui::TreeNode("Blend states"))
					{
						for (size_t i = 0; i < t_Pipeline.renderTargetBlendCount; i++)
						{
							const PipelineRenderTargetBlend& t_BlendInfo = t_Pipeline.renderTargetBlends[i];
							if (ImGui::TreeNode((void*)(intptr_t)i, "Blend State %u:", i))
							{
								ImGui::Text("Blend Enable: %d", t_BlendInfo.blendEnable);
								ImGui::Text("Source Blend", BlendFactor(t_BlendInfo.srcBlend));
								ImGui::Text("Destination Blend", BlendFactor(t_BlendInfo.dstBlend));
								ImGui::Text("Blend Op", BlendOp(t_BlendInfo.blendOp));
								ImGui::Text("Source Blend Alpha", BlendFactor(t_BlendInfo.srcBlendAlpha));
								ImGui::Text("Destination Blend Alpha", BlendFactor(t_BlendInfo.dstBlendAlpha));
								ImGui::Text("Blend Op Alpha", BlendOp(t_BlendInfo.blendOpAlpha));
								ImGui::TreePop();
							}
						}
						ImGui::TreePop();
					}

					if (ImGui::TreeNode("Shaders"))
					{
						for (size_t i = 0; i < t_Pipeline.shaderCount; i++)
						{
							const PipelineDebugInfo::ShaderInfo& t_ShaderInfo = t_Pipeline.shaderInfo[i];
							if (ImGui::TreeNode((void*)(intptr_t)i, "Shader %u:", i))
							{
								if (t_ShaderInfo.optionalShaderpath)
									ImGui::Text(t_ShaderInfo.optionalShaderpath);
								ImGui::Text(ShaderStageStr(t_ShaderInfo.shaderStage));
								ImGui::TreePop();
							}
						}
						ImGui::TreePop();
					}

					if (ImGui::TreeNode("Vertex attributes"))
					{
						for (size_t i = 0; i < t_Pipeline.attributeCount; i++)
						{
							const VertexAttributeDesc& t_Attri = t_Pipeline.attributes[i];
							if (ImGui::TreeNode((void*)(intptr_t)i, "Vertex Attribute %u:", i))
							{
								ImGui::Text("binding: %u", t_Attri.location);
								ImGui::Text(InputFormat(t_Attri.format));
								ImGui::Text("offset: %u", t_Attri.offset);
								ImGui::Text(t_Attri.semanticName);
								ImGui::TreePop();
							}
						}
						ImGui::TreePop();
					}
				}
				break;
				case RESOURCE_TYPE::BUFFER:
				{
					const RenderBufferCreateInfo& t_Buffer =
						*reinterpret_cast<RenderBufferCreateInfo*>(t_Entry->typeInfo);
					ImGui::Text("Size: %u", t_Buffer.size);
					switch (t_Buffer.usage)
					{
					case RENDER_BUFFER_USAGE::VERTEX:
						ImGui::Text("Usage: VERTEX");
						break;
					case RENDER_BUFFER_USAGE::INDEX:
						ImGui::Text("Usage: INDEX");
						break;
					case RENDER_BUFFER_USAGE::UNIFORM:
						ImGui::Text("Usage: UNIFORM");
						break;
					case RENDER_BUFFER_USAGE::STORAGE:
						ImGui::Text("Usage: STORAGE");
						break;
					case RENDER_BUFFER_USAGE::STAGING:
						ImGui::Text("Usage: STAGING");
						break;
					default:
						BB_ASSERT(false, "Unknown RENDER_BUFFER_USAGE for resource trackering editor");
						break;
					}
					switch (t_Buffer.memProperties)
					{
					case RENDER_MEMORY_PROPERTIES::DEVICE_LOCAL:
						ImGui::Text("Memory properties: DEVICE_LOCAL");
						break;
					case RENDER_MEMORY_PROPERTIES::HOST_VISIBLE:
						ImGui::Text("Memory properties: HOST_VISIBLE");
						break;
					default:
						BB_ASSERT(false, "Unknown RENDER_MEMORY_PROPERTIES for resource trackering editor");
						break;
					}
				}
				break;
				case RESOURCE_TYPE::IMAGE:
				{

				}
				break;
				case RESOURCE_TYPE::SAMPLER:
				{

				}
				break;
				case RESOURCE_TYPE::FENCE:
				{

				}
				break;
				default:
					BB_ASSERT(false, "Unknown RESOURCE_TYPE for resource trackering editor");
					break;
				}
			}
			ImGui::PopID();
			t_Entry = t_Entry->next;
		}
	}
	ImGui::End();
}

#pragma endregion //Editor