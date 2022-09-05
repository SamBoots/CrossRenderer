#pragma once

//Source on idea: https://blog.molecular-matters.com/2011/06/29/designing-extensible-modular-classes/

#include <unordered_map>
#include "Utils/Utils.h"
#include "Utils/Logger.h"

namespace BB
{
	namespace MemoryDebugTools
	{
		constexpr const uintptr_t BoundryCheckValue = 0xDEADBEEFDEADBEEF;
		constexpr const size_t BOUNDRY_FRONT = sizeof(size_t);
		constexpr const size_t BOUNDRY_BACK = sizeof(size_t);

		struct BoundsCheck
		{
			~BoundsCheck();

			void AddBoundries(void* a_FrontPtr, size_t a_AllocSize);
			void CheckBoundries(void* a_FrontPtr);
			void Clear();

			//replace with own hashmap.
			//First pointer = front.
			//Second pointer = back.
			std::unordered_map<void*, void*> m_BoundsList;
		};

		struct MemoryTrack
		{
			~MemoryTrack();
			void OnAlloc(void* a_Ptr, size_t a_Size);
			void OnDealloc(void* a_Ptr);
			void Clear();

			//replace with own hashmap.
			//needs replacement by a custom hashmap.
			std::unordered_map<void*, size_t> m_TrackingList;
		};
	}

	namespace ThreadPolicy
	{
		struct Single_Thread
		{
			inline void Enter(void) const {};
			inline void Leave(void) const {};
		};
	}

	typedef void* (*AllocateFunc)(void* a_AllocatorData,  size_t a_Size, size_t a_Alignment, void* a_OldPtr);
	struct Allocator
	{
		AllocateFunc func;
		void* allocator;
	};

	template<typename T>
	void* Realloc(void* a_Allocator, size_t a_Size, size_t a_Alignment, void* a_Ptr)
	{
		if (a_Size > 0)
		{
			return reinterpret_cast<T*>(a_Allocator)->Alloc(a_Size, a_Alignment);
		}
		else
		{
			reinterpret_cast<T*>(a_Allocator)->Free(a_Ptr);
			return nullptr;
		}
	}

	template <class Allocator_Type, class ThreadPolicy, bool debugging>
	struct MemoryArena
	{
		operator Allocator()
		{
			Allocator t_AllocatorInterface;
			t_AllocatorInterface.allocator = this;
			t_AllocatorInterface.func = Realloc<MemoryArena<Allocator_Type, ThreadPolicy, debugging>>;
			return t_AllocatorInterface;
		}

		MemoryArena(const size_t a_Size)
			: m_Allocator(a_Size)
		{}
		
		MemoryArena(const size_t a_ObjectSize, const size_t a_ObjectCount, const size_t a_Alignment)
			: m_Allocator(a_ObjectSize, a_ObjectCount, a_Alignment)
		{}

		void* Alloc(size_t a_Size, size_t a_Alignment)
		{
			m_ThreadPolicy.Enter();

			if constexpr (debugging)
			{
				//Add more room for the boundry checking.
				a_Size += MemoryDebugTools::BOUNDRY_FRONT + MemoryDebugTools::BOUNDRY_BACK;
			} //debugging
			void* allocatedMemory = m_Allocator.Alloc(a_Size, a_Alignment);

			if constexpr (debugging)
			{
				//Do all the debugging tools.
				m_BoundsCheck.AddBoundries(allocatedMemory, a_Size);
				m_MemoryTrack.OnAlloc(allocatedMemory, a_Size);
				allocatedMemory = Pointer::Add(allocatedMemory, MemoryDebugTools::BOUNDRY_FRONT);
			} //debugging
			m_ThreadPolicy.Leave();

			return allocatedMemory;
		}
		void Free(void* a_Ptr)
		{
			m_ThreadPolicy.Enter();
			if constexpr (debugging)
			{
			//Adjust the pointer to the boundry that was being set.
			a_Ptr = Pointer::Subtract(a_Ptr, MemoryDebugTools::BOUNDRY_FRONT);
			m_BoundsCheck.CheckBoundries(a_Ptr);
			m_MemoryTrack.OnDealloc(a_Ptr);
			} //debugging
			m_Allocator.Free(a_Ptr);

			m_ThreadPolicy.Leave();
		}

		void Clear()
		{
			m_ThreadPolicy.Enter();
			if constexpr (debugging)
			{
				m_BoundsCheck.Clear();
				m_MemoryTrack.Clear();
			} //debugging
			m_Allocator.Clear();

			m_ThreadPolicy.Leave();
		}

	protected:
		Allocator_Type m_Allocator;
		ThreadPolicy m_ThreadPolicy;

		MemoryDebugTools::BoundsCheck m_BoundsCheck;
		MemoryDebugTools::MemoryTrack m_MemoryTrack;
	};
}