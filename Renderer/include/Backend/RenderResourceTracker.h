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

		void AddDescriptor(const RenderDescriptorCreateInfo& a_Descriptor, const char* a_Name, const uint64_t a_ID);
		void AddQueue(const RenderCommandQueueCreateInfo& a_Queue, const char* a_Name, const uint64_t a_ID);
		void AddCommandAllocator(const RenderCommandAllocatorCreateInfo& a_CommandAllocator, const char* a_Name, const uint64_t a_ID);
		void AddCommandList(const RenderCommandListCreateInfo& a_CommandList, const char* a_Name, const uint64_t a_ID);
		void AddPipeline(const PipelineDebugInfo& a_Pipeline, const char* a_Name, const uint64_t a_ID);
		void AddBuffer(const RenderBufferCreateInfo& a_Buffer, const char* a_Name, const uint64_t a_ID);
		void AddImage(const RenderImageCreateInfo& a_Image, const char* a_Name, const uint64_t a_ID);
		void AddSampler(const SamplerCreateInfo& a_Sampler, const char* a_Name, const uint64_t a_ID);
		void AddFence(const FenceCreateInfo& a_Fence, const char* a_Name, const uint64_t a_ID);

		void Editor();

		inline void RemoveEntry(const uint64_t a_ID)
		{
			Entry* t_PreviousEntry = nullptr;
			Entry* t_Entry = m_HeadEntry;
			for (size_t i = 0; i < m_Entries; i++)
			{
				BB_ASSERT(t_Entry != nullptr, "Trying to remove an entry but iterating over too many elements!");
				if (t_Entry->id == a_ID)
				{
					if (t_PreviousEntry == nullptr)
					{
						m_HeadEntry = t_Entry->next;
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
			--m_Entries;
		}

		struct Entry
		{
			RESOURCE_TYPE type{};
			uint64_t id;
			const char* name = nullptr;
			Entry* next = nullptr;
			void* typeInfo = nullptr;
		};
		void SortByType();

	private:
		enum SORT_TYPE : uint32_t
		{
			TIME,
			TYPE
		};

		template<typename T>
		inline void AddEntry(const T& a_TypeInfo, const RESOURCE_TYPE a_Type, const char* a_Name, const uint64_t a_ID)
		{
			const size_t t_EntrySize = sizeof(T) + sizeof(Entry);
			Entry* t_Entry = reinterpret_cast<Entry*>(BBalloc(m_Allocator, t_EntrySize));
			t_Entry->type = a_Type;
			t_Entry->id = a_ID;
			t_Entry->name = a_Name;
			t_Entry->typeInfo = Pointer::Add(t_Entry, sizeof(Entry));
			T* t_Obj = reinterpret_cast<T*>(t_Entry->typeInfo);
			*t_Obj = a_TypeInfo;
			++m_Entries;
			switch (m_SortType)
			{
			case BB::RenderResourceTracker::TIME:
				t_Entry->next = m_HeadEntry;
				m_HeadEntry = t_Entry;
				break;
			case BB::RenderResourceTracker::TYPE:
			{
				Entry* t_SearchEntry = m_HeadEntry;
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

		}

		FreelistAllocator_t m_Allocator{ mbSize * 2 };

		SORT_TYPE m_SortType = SORT_TYPE::TIME;
		uint32_t m_Entries = 0;
		Entry* m_HeadEntry = nullptr;
	};
}