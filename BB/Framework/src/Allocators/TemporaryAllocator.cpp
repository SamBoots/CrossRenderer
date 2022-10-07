#include "TemporaryAllocator.h"

namespace BB
{
	void* ReallocTemp(BB_MEMORY_DEBUG void* a_Allocator, size_t a_Size, size_t a_Alignment, void*)
	{
		if (a_Size == 0)
			return nullptr;

		return reinterpret_cast<TemporaryAllocator*>(a_Allocator)->Alloc(a_Size, a_Alignment);
	}

	struct TemporaryFreeBlock
	{
		size_t size;
		size_t used;
		TemporaryFreeBlock* previousBlock;
	};

	BB::TemporaryAllocator::operator BB::Allocator()
	{
		Allocator t_AllocatorInterface;
		t_AllocatorInterface.allocator = this;
		t_AllocatorInterface.func = ReallocTemp;
		return t_AllocatorInterface;
	}

	BB::TemporaryAllocator::TemporaryAllocator(Allocator a_BackingAllocator)
	{
		m_BackingAllocator = a_BackingAllocator;
		m_FreeBlock = reinterpret_cast<TemporaryFreeBlock*>(m_Buffer);
		m_FreeBlock->size = sizeof(m_Buffer);
		m_FreeBlock->used = sizeof(TemporaryFreeBlock);
		m_FreeBlock->previousBlock = nullptr;
	}

	BB::TemporaryAllocator::~TemporaryAllocator()
	{
		while (m_FreeBlock->previousBlock != nullptr)
		{
			TemporaryFreeBlock* t_PreviousBlock = m_FreeBlock->previousBlock;
			BBfree(m_BackingAllocator, m_FreeBlock);
			m_FreeBlock = t_PreviousBlock;
		}
	}

	void* BB::TemporaryAllocator::Alloc(size_t a_Size, size_t a_Alignment)
	{
		size_t t_Adjustment = Pointer::AlignForwardAdjustment(
			Pointer::Add(m_FreeBlock, m_FreeBlock->used),
			a_Alignment);

		size_t t_AlignedSize = a_Size + t_Adjustment;

		//Does it fit in our current block.
		if (m_FreeBlock->size - m_FreeBlock->used >= t_AlignedSize)
		{
			void* t_Address = Pointer::Add(m_FreeBlock, m_FreeBlock->used);
			m_FreeBlock->used += t_AlignedSize;
			return t_Address;
		}

		//Create new one and double the blocksize
		size_t t_NewBlockSize = m_FreeBlock->used + m_FreeBlock->size;
		//If the allocation is bigger then the new block resize the new block for the allocation.
		//Round up to the new block size for ease of use and correct alignment.
		if (t_AlignedSize > t_NewBlockSize)
			t_NewBlockSize = Math::RoundUp(t_AlignedSize, t_NewBlockSize);

		TemporaryFreeBlock* t_Previous = m_FreeBlock;
		m_FreeBlock = reinterpret_cast<TemporaryFreeBlock*>(BBalloc(m_BackingAllocator, t_NewBlockSize));
		m_FreeBlock->size = t_NewBlockSize;
		m_FreeBlock->used = sizeof(TemporaryFreeBlock);
		m_FreeBlock->previousBlock = t_Previous;

		//Try again with the new block
		return Alloc(a_Size, a_Alignment);
	}

	void BB::TemporaryAllocator::Clear()
	{
		while (m_FreeBlock->previousBlock != nullptr)
		{
			TemporaryFreeBlock* t_PreviousBlock = m_FreeBlock->previousBlock;
			BBfree(m_BackingAllocator, m_FreeBlock);
			m_FreeBlock = t_PreviousBlock;
		}
		m_FreeBlock->size = sizeof(m_Buffer);
		m_FreeBlock->used = sizeof(TemporaryFreeBlock);
	}
}