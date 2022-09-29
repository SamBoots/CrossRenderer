#include "Utils/Logger.h"
#include "BBMemory.h"

#include <iostream>

using namespace BB;

#pragma region MemoryArena_Debugging

constexpr const uintptr_t BoundryCheckValue = 0xDEADBEEFDEADBEEF;
constexpr const size_t BOUNDRY_FRONT = sizeof(size_t);
constexpr const size_t BOUNDRY_BACK = sizeof(size_t);

struct BoundsCheck
{
	~BoundsCheck()
	{
		for (auto& t_It : m_BoundsList)
		{
			//Set the begin bound value
			BB_ASSERT(*reinterpret_cast<size_t*>(t_It.first) == BoundryCheckValue, "Memory boundrycheck failed! Buffer overwritten at the front.");
			BB_ASSERT(*reinterpret_cast<size_t*>(t_It.second) == BoundryCheckValue, "Memory boundrycheck failed! Buffer overwritten at the back.");
		}
		m_BoundsList.clear();
	}
	void AddBoundries(void* a_FrontPtr, size_t a_AllocSize)
	{
		//Set the begin bound value
		*reinterpret_cast<size_t*>(a_FrontPtr) = BoundryCheckValue;

		void* a_BackPtr = Pointer::Add(a_FrontPtr, a_AllocSize - BOUNDRY_BACK);
		*reinterpret_cast<size_t*>(a_BackPtr) = BoundryCheckValue;

		m_BoundsList.emplace(a_FrontPtr, a_BackPtr);
	}
	void CheckBoundries(void* a_FrontPtr)
	{
		//Set the begin bound value
		BB_ASSERT(*reinterpret_cast<size_t*>(a_FrontPtr) == BoundryCheckValue, "Memory boundrycheck failed! Buffer overwritten at the front.");
		BB_ASSERT(*reinterpret_cast<size_t*>(m_BoundsList.find(a_FrontPtr)->second) == BoundryCheckValue, "Memory boundrycheck failed! Buffer overwritten at the back.");

		m_BoundsList.erase(a_FrontPtr);
	}
	void Clear()
	{
		for (auto& t_It : m_BoundsList)
		{
			//Set the begin bound value
			BB_ASSERT(*reinterpret_cast<size_t*>(t_It.first) == BoundryCheckValue, "Memory boundrycheck failed! Buffer overwritten at the front.");
			BB_ASSERT(*reinterpret_cast<size_t*>(t_It.second) == BoundryCheckValue, "Memory boundrycheck failed! Buffer overwritten at the back.");
		}
		m_BoundsList.clear();
	}

	//replace with own hashmap.
	//First pointer = front.
	//Second pointer = back.
	std::unordered_map<void*, void*> m_BoundsList;
};

struct MemoryTrack
{
	~MemoryTrack()
	{
		for (auto& t_It : m_TrackingList)
		{
			std::cout << "Address: " << t_It.first << " Leak size: " << t_It.second << "\n";
		}
		BB_WARNING(m_TrackingList.size() == 0, "Memory tracker reports a memory leak, Log of leaks have been posted.", WarningType::HIGH);
	}
	void OnAlloc(void* a_Ptr, size_t a_Size)
	{
		m_TrackingList.emplace(a_Ptr, a_Size);
	}
	void OnDealloc(void* a_Ptr)
	{
		m_TrackingList.erase(a_Ptr);
	}
	void Clear()
	{
		m_TrackingList.clear();
	}


	//replace with own hashmap.
	//needs replacement by a custom hashmap.
	std::unordered_map<void*, size_t> m_TrackingList;
};

struct MemoryArena
{
	BoundsCheck boundCheck;
	MemoryTrack memTrack;
};

static std::unordered_map<const void*, MemoryArena> s_MemoryArenas;


void BB::CreateMemoryDebugArena(const void* a_AllocatorAddress)
{
	s_MemoryArenas.emplace(a_AllocatorAddress, MemoryArena());
}

void BB::DestroyMemoryDebugArena(const void* a_AllocatorAddress)
{
	s_MemoryArenas.erase(a_AllocatorAddress);
}

void BB::ClearMemoryDebugArena(const void* a_AllocatorAddress)
{
	MemoryArena& t_Arena = s_MemoryArenas.at(a_AllocatorAddress);
	t_Arena.boundCheck.Clear();
	t_Arena.memTrack.Clear();
}

void BB::AllocMemoryDebugAdjustBoundrySize(size_t& a_Size)
{
	a_Size += BOUNDRY_FRONT + BOUNDRY_BACK;
}

void* BB::AllocMemoryDebug(const void* a_AllocatorAddress, void* a_AllocatedPtr, const size_t a_Size, const size_t a_Alignment)
{
	MemoryArena& t_Arena = s_MemoryArenas.at(a_AllocatorAddress);
	size_t t_BoundAdjustedSize = a_Size + BOUNDRY_FRONT + BOUNDRY_BACK;

	t_Arena.boundCheck.AddBoundries(a_AllocatedPtr, a_Size);
	t_Arena.memTrack.OnAlloc(a_AllocatedPtr, a_Size);

	return Pointer::Add(a_AllocatedPtr, BOUNDRY_FRONT);
}

//Returns the real memory address.
void* BB::FreeMemoryDebug(const void* a_AllocatorAddress, const void* a_AllocPtr)
{
	MemoryArena& t_Arena = s_MemoryArenas.at(a_AllocatorAddress);

	//Adjust the pointer to the boundry that was being set.
	void* t_BoundsPtr = Pointer::Subtract(a_AllocPtr, BOUNDRY_FRONT);
	t_Arena.boundCheck.CheckBoundries(t_BoundsPtr);
	t_Arena.memTrack.OnDealloc(t_BoundsPtr);

	return t_BoundsPtr;
}

#pragma endregion MemoryArena_Debugging