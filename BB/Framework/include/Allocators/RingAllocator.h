#pragma once
#include "BBMemory.h"

namespace BB
{
	//A simple ring allocator that allocates until it reaches it's maximum, then it overwrites previous elements at the start again. Careful when using this.
	//A ring allocator that uses a different allocator to allocate it's memory.
	class RingAllocator
	{
	public:
		operator Allocator();

		RingAllocator(const Allocator a_BackingAllocator, const size_t a_Size);
		~RingAllocator();

		//just delete these for safety, copies might cause errors.
		RingAllocator(const RingAllocator&) = delete;
		RingAllocator(const RingAllocator&&) = delete;
		RingAllocator& operator =(const RingAllocator&) = delete;
		RingAllocator& operator =(RingAllocator&&) = delete;

		void* Alloc(size_t a_Size, size_t a_Alignment);

	private:
		//use uint32_t so that the class size becomes 32 bytes. 
		//The ring buffer won't get over UINT32_MAX anyway. Else you fucking up!
		void* m_BufferPos;
		const uint32_t m_Size;
		uint32_t m_Used;

		//Remember out backing allocator so that we automatically free our memory.
		const Allocator m_BackingAllocator;
	};

	//A simple ring allocator that allocates until it reaches it's maximum, then it overwrites previous elements at the start again. Careful when using this.
	//A ring allocator that uses virtual alloc to get it's own memory.
	class LocalRingAllocator
	{
	public:
		operator Allocator();

		//Just giving it a size will use virtual_alloc
		//a_Size must be a value above 0, but it will return the actual page aligned size of the allocator.
		LocalRingAllocator(size_t& a_Size);
		~LocalRingAllocator();

		//just delete these for safety, copies might cause errors.
		LocalRingAllocator(const LocalRingAllocator&) = delete;
		LocalRingAllocator(const LocalRingAllocator&&) = delete;
		LocalRingAllocator& operator =(const LocalRingAllocator&) = delete;
		LocalRingAllocator& operator =(LocalRingAllocator&&) = delete;

		void* Alloc(size_t a_Size, size_t a_Alignment);

	private:
		void* m_BufferPos;
		size_t m_Size;
		size_t m_Used;
	};
}
