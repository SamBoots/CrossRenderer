#pragma once
#include "RenderBackendCommon.h"

namespace BB
{
	class Editor;

	enum class RESOURCE_TYPE
	{
		DESCRIPTOR,
		COMMAND_QUEUE,
		COMMAND_ALLOCATOR,
		COMMAND_LIST,
		PIPELINE,
		BUFFER,
		IMAGE,
		SAMPLER,
		FENCE
	};
	
	class RenderResourceTracker
	{
	public:
		//editor will display the values that are private here.
		friend class BB::Editor;

		RenderResourceTracker() {};
		~RenderResourceTracker() {};

		void AddDescriptor(const RenderDescriptorCreateInfo& a_Descriptor, const char* a_Name);
		void AddQueue(const RenderCommandQueueCreateInfo& a_Queue, const char* a_Name);
		void AddCommandAllocator(const RenderCommandAllocatorCreateInfo& a_CommandAllocator, const char* a_Name);
		void AddCommandList(const RenderCommandListCreateInfo& a_CommandList, const char* a_Name);
		void AddPipeline(const PipelineDebugInfo& a_Pipeline, const char* a_Name);
		void AddBuffer(const RenderBufferCreateInfo& a_Buffer, const char* a_Name);
		void AddImage(const RenderImageCreateInfo& a_Image, const char* a_Name);
		void AddSampler(const SamplerCreateInfo& a_Sampler, const char* a_Name);
		void AddFence(const FenceCreateInfo& a_Fence, const char* a_Name);

		void Editor() const;

	private:
		struct Entry
		{
			RESOURCE_TYPE type{};
			const char* name = nullptr;
			Entry* next = nullptr;
			void* typeInfo = nullptr;
		};

		template<typename T>
		inline void AddEntry(const T& a_TypeInfo, const RESOURCE_TYPE a_Type, const char* a_Name)
		{
			const size_t t_EntrySize = sizeof(T) + sizeof(Entry);
			Entry* t_Entry = reinterpret_cast<Entry*>(BBalloc(m_Allocator, t_EntrySize));
			t_Entry->type = a_Type;
			t_Entry->name = a_Name;
			t_Entry->next = m_HeadEntry;
			t_Entry->typeInfo = Pointer::Add(t_Entry, sizeof(Entry));
			T* t_Obj = reinterpret_cast<T*>(t_Entry->typeInfo); 
			*t_Obj = a_TypeInfo;
			++m_Entries;
			m_HeadEntry = t_Entry;
		}

		FreelistAllocator_t m_Allocator{ mbSize * 2 };

		uint32_t m_Entries = 0;
		Entry* m_HeadEntry = nullptr;
	};
}