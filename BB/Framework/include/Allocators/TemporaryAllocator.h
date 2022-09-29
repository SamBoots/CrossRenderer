#pragma once
#include "BBMemory.h"
namespace BB
{
	//A linear allocator abstraction that doesn't use virtual alloc but another allocator for memory.
	struct TemporaryAllocator
	{
		operator Allocator();

		TemporaryAllocator(Allocator a_BackingAllocator);
		~TemporaryAllocator();

		//just delete these for safety, copies might cause errors.
		TemporaryAllocator(const TemporaryAllocator&) = delete;
		TemporaryAllocator(const TemporaryAllocator&&) = delete;
		TemporaryAllocator& operator =(const TemporaryAllocator&) = delete;
		TemporaryAllocator& operator =(TemporaryAllocator&&) = delete;

		void* Alloc(size_t a_Size, size_t a_Alignment);
		void Clear();

	private:
		uint8_t m_Buffer[1024];

		Allocator m_BackingAllocator;
		struct TemporaryFreeBlock* m_FreeBlock;
	};
}