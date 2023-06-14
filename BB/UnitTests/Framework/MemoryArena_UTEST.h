#pragma once
#include "../TestValues.h"
#include "BBMemory.h"

TEST(MemoryTesting, Create_Memory_Leak)
{
	constexpr size_t allocatorSize = 1028;
	constexpr size_t allocationSize = 256;

	BB::LinearAllocator_t t_LinearAllocator(allocatorSize, "Leak tester");

	BBalloc(t_LinearAllocator, allocationSize);
	//Leak will accur.
}