#include "Utils/Logger.h"
#include "MemoryArena.h"

#include <iostream>

using namespace BB::MemoryDebugTools;

BoundsCheck::~BoundsCheck()
{
	for (auto& t_It : m_BoundsList)
	{
		//Set the begin bound value
		BB_ASSERT(*reinterpret_cast<size_t*>(t_It.first) == BoundryCheckValue, "Memory boundrycheck failed! Buffer overwritten at the front.");
		BB_ASSERT(*reinterpret_cast<size_t*>(t_It.second) == BoundryCheckValue, "Memory boundrycheck failed! Buffer overwritten at the back.");
	}
	m_BoundsList.clear();
}

void BoundsCheck::AddBoundries(void* a_FrontPtr, size_t a_AllocSize)
{
	//Set the begin bound value
	*reinterpret_cast<size_t*>(a_FrontPtr) = BoundryCheckValue;

	void* a_BackPtr = Pointer::Add(a_FrontPtr, a_AllocSize - BOUNDRY_BACK);
	*reinterpret_cast<size_t*>(a_BackPtr) = BoundryCheckValue;

	m_BoundsList.emplace(a_FrontPtr, a_BackPtr);
}

void BoundsCheck::CheckBoundries(void* a_FrontPtr)
{
	//Set the begin bound value
	BB_ASSERT(*reinterpret_cast<size_t*>(a_FrontPtr) == BoundryCheckValue, "Memory boundrycheck failed! Buffer overwritten at the front.");
	BB_ASSERT(*reinterpret_cast<size_t*>(m_BoundsList.find(a_FrontPtr)->second) == BoundryCheckValue, "Memory boundrycheck failed! Buffer overwritten at the back.");

	m_BoundsList.erase(a_FrontPtr);
}

void BoundsCheck::Clear()
{
	for (auto& t_It : m_BoundsList)
	{
		//Set the begin bound value
		BB_ASSERT(*reinterpret_cast<size_t*>(t_It.first) == BoundryCheckValue, "Memory boundrycheck failed! Buffer overwritten at the front.");
		BB_ASSERT(*reinterpret_cast<size_t*>(t_It.second) == BoundryCheckValue, "Memory boundrycheck failed! Buffer overwritten at the back.");
	}
	m_BoundsList.clear();
}

MemoryTrack::~MemoryTrack()
{
	for (auto& t_It : m_TrackingList)
	{
		std::cout << "Address: " << t_It.first << " Leak size: " << t_It.second << "\n";
	}
	BB_WARNING(m_TrackingList.size() == 0, "Memory tracker reports a memory leak, Log of leaks have been posted.", WarningType::HIGH);
}

void MemoryTrack::OnAlloc(void* a_Ptr, size_t a_Size)
{
	m_TrackingList.emplace(a_Ptr, a_Size);
}
void MemoryTrack::OnDealloc(void* a_Ptr)
{
	m_TrackingList.erase(a_Ptr);
}
void MemoryTrack::Clear()
{
	m_TrackingList.clear();
}