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

struct DescriptorDebugInfo
{
	const char* name = nullptr;
	uint32_t bindingCount;
	DescriptorBinding* bindings;
};

struct BB::RenderResourceTracker_Inst
{
	RenderResourceTracker_Inst(Allocator a_Allocator) : entryMap(a_Allocator, 1028) {};

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
	inst = BBnew(m_Allocator, RenderResourceTracker_Inst)(m_Allocator);
}

BB::RenderResourceTracker::~RenderResourceTracker()
{

	BBfree(m_Allocator, inst);
}

void BB::RenderResourceTracker::AddDescriptor(const RenderDescriptorCreateInfo& a_Descriptor, const char* a_Name, const uint64_t a_ID)
{
	constexpr size_t t_EntrySize = sizeof(DescriptorDebugInfo) + sizeof(Entry);
	const size_t t_BindingAllocSize = a_Descriptor.bindings.sizeInBytes();
	const size_t t_AllocSize = t_EntrySize + t_BindingAllocSize;

	Entry* t_Entry = reinterpret_cast<Entry*>(BBalloc(m_Allocator, t_AllocSize));
	t_Entry->type = RESOURCE_TYPE::DESCRIPTOR;
	t_Entry->timeId = inst->timeID++;
	t_Entry->id = a_ID;
	t_Entry->name = a_Name;
	t_Entry->next = inst->headEntry;
	t_Entry->typeInfo = Pointer::Add(t_Entry, sizeof(Entry));
	DescriptorDebugInfo* t_DescDebug = reinterpret_cast<DescriptorDebugInfo*>(t_Entry->typeInfo);
	t_DescDebug->name = a_Name;
	t_DescDebug->bindingCount = static_cast<uint32_t>(a_Descriptor.bindings.size());
	t_DescDebug->bindings = reinterpret_cast<DescriptorBinding*>(Pointer::Add(t_Entry, t_EntrySize));
	Memory::Copy(t_DescDebug->bindings, a_Descriptor.bindings.data(), t_DescDebug->bindingCount);


	++inst->entries;
	inst->headEntry = t_Entry;
	inst->entryMap.insert(t_Entry->id, t_Entry);
}

void BB::RenderResourceTracker::AddQueue(const RenderCommandQueueCreateInfo& a_Queue, const char* a_Name, const uint64_t a_ID)
{
	inst->AddEntry(m_Allocator, a_Queue, RESOURCE_TYPE::COMMAND_QUEUE, a_Name, a_ID);
}

void BB::RenderResourceTracker::AddCommandAllocator(const RenderCommandAllocatorCreateInfo& a_CommandAllocator, const char* a_Name, const uint64_t a_ID)
{
	inst->AddEntry(m_Allocator, a_CommandAllocator, RESOURCE_TYPE::COMMAND_ALLOCATOR, a_Name, a_ID);
}

void BB::RenderResourceTracker::AddCommandList(const RenderCommandListCreateInfo& a_CommandList, const char* a_Name, const uint64_t a_ID)
{
	inst->AddEntry(m_Allocator, a_CommandList, RESOURCE_TYPE::COMMAND_LIST, a_Name, a_ID);
}

void BB::RenderResourceTracker::AddPipeline(const PipelineDebugInfo& a_Pipeline, const char* a_Name, const uint64_t a_ID)
{
	//Pipeline has multiple dynamic entries so account for that.
	constexpr size_t t_EntrySize = sizeof(PipelineDebugInfo) + sizeof(Entry);
	const size_t t_ShaderInfoSize = a_Pipeline.shaderCount * sizeof(PipelineDebugInfo::ShaderInfo);
	const size_t t_AttributeSize = a_Pipeline.attributeCount * sizeof(VertexAttributeDesc);
	const size_t t_ImmutableSamplerSize = a_Pipeline.immutableSamplerCount * sizeof(SamplerCreateInfo);
	const size_t t_AllocSize = t_EntrySize + t_ShaderInfoSize + t_AttributeSize + t_ImmutableSamplerSize;


	Entry* t_Entry = reinterpret_cast<Entry*>(BBalloc(m_Allocator, t_AllocSize));
	t_Entry->type = RESOURCE_TYPE::PIPELINE;
	t_Entry->timeId = inst->timeID++;
	t_Entry->id = a_ID;
	t_Entry->name = a_Name;
	t_Entry->next = inst->headEntry;
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
	for (size_t i = 0; i < a_Pipeline.immutableSamplerCount; i++)
	{
		t_PipelineInfo->immutableSamplers = reinterpret_cast<SamplerCreateInfo*>(Pointer::Add(t_Entry, t_EntrySize + t_ShaderInfoSize + t_AttributeSize));
		t_PipelineInfo->immutableSamplers[i] = a_Pipeline.immutableSamplers[i];
	}

	++inst->entries;
	inst->headEntry = t_Entry;
	inst->entryMap.insert(t_Entry->id, t_Entry);
}

void BB::RenderResourceTracker::AddBuffer(const RenderBufferCreateInfo& a_Buffer, const char* a_Name, const uint64_t a_ID)
{
	inst->AddEntry(m_Allocator, a_Buffer, RESOURCE_TYPE::BUFFER, a_Name, a_ID);
}

void BB::RenderResourceTracker::AddImage(const TrackerImageInfo& a_Image, const char* a_Name, const uint64_t a_ID)
{
	inst->AddEntry(m_Allocator, a_Image, RESOURCE_TYPE::IMAGE, a_Name, a_ID);
}

void BB::RenderResourceTracker::AddSampler(const SamplerCreateInfo& a_Sampler, const char* a_Name, const uint64_t a_ID)
{
	inst->AddEntry(m_Allocator, a_Sampler, RESOURCE_TYPE::SAMPLER, a_Name, a_ID);
}

void BB::RenderResourceTracker::AddFence(const FenceCreateInfo& a_Fence, const char* a_Name, const uint64_t a_ID)
{
	inst->AddEntry(m_Allocator, a_Fence, RESOURCE_TYPE::FENCE, a_Name, a_ID);
}

void* BB::RenderResourceTracker::GetData(const uint64_t a_ID, const RESOURCE_TYPE a_Type)
{
	Entry* t_Entry = *inst->entryMap.find(a_ID);
	BB_ASSERT(t_Entry->type == a_Type, "Trying to get a resource but the ID is not equal to it's type!");
	return t_Entry->typeInfo;
}

void BB::RenderResourceTracker::ChangeName(const uint64_t a_ID, const char* a_NewName)
{
	Entry* t_Entry = *inst->entryMap.find(a_ID);
	t_Entry->name = a_NewName;
}

void BB::RenderResourceTracker::RemoveEntry(const uint64_t a_ID)
{
	Entry* t_PreviousEntry = nullptr;
	Entry* t_Entry = inst->headEntry;
	for (size_t i = 0; i < inst->entries; i++)
	{
		BB_ASSERT(t_Entry != nullptr, "Trying to remove an entry but iterating over too many elements!");
		if (t_Entry->id == a_ID)
		{
			if (t_PreviousEntry == nullptr)
			{
				inst->headEntry = t_Entry->next;
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
	--inst->entries;
	inst->entryMap.erase(t_Entry->id);
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
	inst->sortType = SORT_TYPE::TYPE;
	SortEntriesByType(&inst->headEntry);
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
	inst->sortType = SORT_TYPE::TIME;
	SortEntriesByTime(&inst->headEntry);
}

#pragma region Editor

#include "imgui.h"
#include "RenderBackendCommon.inl"
#include "Editor.h"

//Does not show the name.
static inline void ShowSamplerImgui(const SamplerCreateInfo& a_Sampler)
{
	ImGui::Text("U Address: %s", SamplerAddressStr(a_Sampler.addressModeU));
	ImGui::Text("V Address: %s", SamplerAddressStr(a_Sampler.addressModeV));
	ImGui::Text("W Address: %s", SamplerAddressStr(a_Sampler.addressModeW));
	ImGui::Text("Filter: %s", SamplerFilterStr(a_Sampler.filter));
	ImGui::Text("Anistoropy: %.6f", a_Sampler.maxAnistoropy);
	ImGui::Text("Min LOD: %.6f", a_Sampler.minLod);
	ImGui::Text("Max LOD: %.6f", a_Sampler.maxLod);
}

void BB::Editor::DisplayRenderResources(BB::RenderResourceTracker& a_ResTracker)
{
	if (!g_ShowEditor)
		return;

	RenderResourceTracker_Inst* t_Inst = a_ResTracker.inst;

	if (ImGui::CollapsingHeader("Render resources"))
	{
		ImGui::Indent();
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
					const DescriptorDebugInfo& t_Desc =
						*reinterpret_cast<DescriptorDebugInfo*>(t_Entry->typeInfo);

					for (size_t i = 0; i < t_Desc.bindingCount; i++)
					{
						const DescriptorBinding& t_Bind = t_Desc.bindings[i];
						if (ImGui::TreeNode((void*)(intptr_t)i, "Binding: %u", t_Bind.binding))
						{
							ImGui::Text("Descriptor Count: %u", t_Bind.descriptorCount);
							ImGui::Text("Descriptor Type: %s", DescriptorTypeStr(t_Bind.type));
							ImGui::Text("Shader Stage: %s", ShaderStageStr(t_Bind.stage));
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
					case RENDER_QUEUE_TYPE::TRANSFER:
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
					case RENDER_QUEUE_TYPE::TRANSFER:
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
							ImGui::Text("ShaderStage: %s", ShaderStageStr(t_Pipeline.constantData.shaderStage));
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
								ImGui::Text("Source Blend", BlendFactorStr(t_BlendInfo.srcBlend));
								ImGui::Text("Destination Blend", BlendFactorStr(t_BlendInfo.dstBlend));
								ImGui::Text("Blend Op", BlendOpStr(t_BlendInfo.blendOp));
								ImGui::Text("Source Blend Alpha", BlendFactorStr(t_BlendInfo.srcBlendAlpha));
								ImGui::Text("Destination Blend Alpha", BlendFactorStr(t_BlendInfo.dstBlendAlpha));
								ImGui::Text("Blend Op Alpha", BlendOpStr(t_BlendInfo.blendOpAlpha));
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
								ImGui::Text("INPUT FORMAT: %s", InputFormatStr(t_Attri.format));
								ImGui::Text("offset: %u", t_Attri.offset);
								ImGui::Text(t_Attri.semanticName);
								ImGui::TreePop();
							}
						}
						ImGui::TreePop();
					}

					if (ImGui::TreeNode("Immutable Samplers"))
					{
						for (size_t i = 0; i < t_Pipeline.immutableSamplerCount; i++)
						{
							const SamplerCreateInfo& t_Sampler = t_Pipeline.immutableSamplers[i];
							if (ImGui::TreeNode(t_Sampler.name))
							{
								ShowSamplerImgui(t_Sampler);
								ImGui::TreePop();
							}
						}
						ImGui::TreePop();
					}
				}
				break;
				case RESOURCE_TYPE::BUFFER:
				{
					const RenderBufferCreateInfo& t_Buffer = *reinterpret_cast<RenderBufferCreateInfo*>(t_Entry->typeInfo);
					ImGui::Text("Size: %u", t_Buffer.size);
					switch (t_Buffer.usage)
					{
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
					case RENDER_BUFFER_USAGE::VERTEX:
						ImGui::Text("Usage: VERTEX");
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
					const TrackerImageInfo& t_Image =
						*reinterpret_cast<TrackerImageInfo*>(t_Entry->typeInfo);
					ImGui::Text("Old Layout %s", ImageLayoutStr(t_Image.oldLayout));
					ImGui::Text("New Layout %s", ImageLayoutStr(t_Image.currentLayout));
					if (ImGui::TreeNode((void*)(intptr_t)t_Entry->timeId, "CreateInfo", t_Entry->timeId))
					{
						const RenderImageCreateInfo& t_CreateInfo = t_Image.createInfo;
						ImGui::Text("Width: %u", t_CreateInfo.width);
						ImGui::Text("Height: %u", t_CreateInfo.height);
						ImGui::Text("Depth: %u", t_CreateInfo.depth);
						ImGui::Text("arrayLayers: %u", t_CreateInfo.arrayLayers);
						ImGui::Text("mipLevels: %u", t_CreateInfo.mipLevels);
						ImGui::Text(ImageTypeStr(t_CreateInfo.type));
						ImGui::Text(ImageFormatStr(t_CreateInfo.format));
						ImGui::Text(ImageTilingStr(t_CreateInfo.tiling));
						ImGui::TreePop();
					}

				}
				break;
				case RESOURCE_TYPE::SAMPLER:
				{
					const SamplerCreateInfo& t_Sampler =
						*reinterpret_cast<SamplerCreateInfo*>(t_Entry->typeInfo);

					ShowSamplerImgui(t_Sampler);
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
		ImGui::Unindent();
	}
}

#pragma endregion //Editor