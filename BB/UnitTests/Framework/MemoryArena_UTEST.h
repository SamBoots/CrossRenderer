#pragma once
#include "../TestValues.h"
#include "BBMemory.h"

TEST(MemoryTesting, Create_Memory_Leak_and_tag)
{
	constexpr size_t allocatorSize = 1028;
	constexpr size_t allocationSize = 256;

	BB::LinearAllocator_t t_LinearAllocator(allocatorSize, "Leak tester");

	void* t_Ptr = BBalloc(t_LinearAllocator, allocationSize);
	BB::BBTagAlloc(t_LinearAllocator, t_Ptr, "memory leak tag");
	//Leak will accur.
}