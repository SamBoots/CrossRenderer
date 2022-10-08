#include "Utils/Logger.h"
#include "BBMemory.h"

using namespace BB;

constexpr const uintptr_t MEMORY_BOUNDRY_CHECK_VALUE = 0xDEADBEEFDEADBEEF;

void* BB::Memory_AddBoundries(void* a_Front, size_t a_AllocSize)
{
	//Set the begin bound value
	*reinterpret_cast<size_t*>(a_Front) = MEMORY_BOUNDRY_CHECK_VALUE;

	//Set the back bytes.
	void* a_Back = Pointer::Add(a_Front, a_AllocSize - MEMORY_BOUNDRY_BACK);
	*reinterpret_cast<size_t*>(a_Back) = MEMORY_BOUNDRY_CHECK_VALUE;

	return a_Back;
}

void BB::Memory_CheckBoundries(void* a_Front, void* a_Back)
{
	BB_ASSERT(*reinterpret_cast<size_t*>(a_Front) == MEMORY_BOUNDRY_CHECK_VALUE, 
		"Memory boundrycheck failed! Buffer overwritten at the front.");
	BB_ASSERT(*reinterpret_cast<size_t*>(a_Back) == MEMORY_BOUNDRY_CHECK_VALUE, 
		"Memory boundrycheck failed! Buffer overwritten at the back.");
}