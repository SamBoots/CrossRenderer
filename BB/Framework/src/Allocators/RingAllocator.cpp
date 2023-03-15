#include "RingAllocator.h"
#include "BackingAllocator.h"

using namespace BB;

void* ReallocRing(BB_MEMORY_DEBUG void* a_Allocator, size_t a_Size, size_t a_Alignment, void*)
{
	if (a_Size == 0)
		return nullptr;

	return reinterpret_cast<RingAllocator*>(a_Allocator)->Alloc(a_Size, a_Alignment);
}

RingAllocator::operator BB::Allocator()
{
	Allocator t_AllocatorInterface;
	t_AllocatorInterface.allocator = this;
	t_AllocatorInterface.func = ReallocRing;
	return t_AllocatorInterface;
}

RingAllocator::RingAllocator(const Allocator a_BackingAllocator, const size_t a_Size)
	:	m_Size(static_cast<uint32_t>(a_Size)), m_BackingAllocator(a_BackingAllocator)
{
	BB_ASSERT(m_Size < UINT32_MAX, 
		"Ring allocator's size is larger then UINT32_MAX. This will not work as the counters inside are 32 bit intergers.!");
	
	m_BufferPos = BBalloc(m_BackingAllocator, a_Size);
	m_Used = 0;
}

RingAllocator::~RingAllocator()
{
	m_BufferPos = Pointer::Subtract(m_BufferPos, m_Used);
	BBfree(m_BackingAllocator, m_BufferPos);
}

void* RingAllocator::Alloc(size_t a_Size, size_t a_Alignment)
{
	size_t t_Adjustment = Pointer::AlignForwardAdjustment(m_BufferPos, a_Alignment);
	size_t t_AdjustedSize = a_Size + a_Alignment;
	//Go back to the buffer start if we cannot fill this allocation.
	if (m_Used + t_AdjustedSize > m_Size)
	{
		BB_ASSERT(m_Size > t_AdjustedSize,
			"Ring allocator tries to allocate something bigger then it's allocator size!");

		m_BufferPos = Pointer::Subtract(m_BufferPos, m_Used);
		m_Used = 0;
		return Alloc(a_Size, a_Alignment);
	}

	void* t_ReturnPtr = Pointer::Add(m_BufferPos, t_Adjustment);
	m_BufferPos = Pointer::Add(m_BufferPos, t_AdjustedSize);
	m_Used += static_cast<uint32_t>(t_AdjustedSize);

	return t_ReturnPtr;
}




void* ReallocLocalRing(BB_MEMORY_DEBUG void* a_Allocator, size_t a_Size, size_t a_Alignment, void*)
{
	if (a_Size == 0)
		return nullptr;

	return reinterpret_cast<LocalRingAllocator*>(a_Allocator)->Alloc(a_Size, a_Alignment);
}

LocalRingAllocator::operator BB::Allocator()
{
	Allocator t_AllocatorInterface;
	t_AllocatorInterface.allocator = this;
	t_AllocatorInterface.func = ReallocLocalRing;
	return t_AllocatorInterface;
}

LocalRingAllocator::LocalRingAllocator(size_t& a_Size)
{
	m_BufferPos = mallocVirtual(nullptr, a_Size, VIRTUAL_RESERVE_NONE);

	m_Size = a_Size;
	m_Used = 0;
}

LocalRingAllocator::~LocalRingAllocator()
{
	m_BufferPos = Pointer::Subtract(m_BufferPos, m_Used);
	freeVirtual(m_BufferPos);
}

void* LocalRingAllocator::Alloc(size_t a_Size, size_t a_Alignment)
{
	size_t t_Adjustment = Pointer::AlignForwardAdjustment(m_BufferPos, a_Alignment);
	size_t t_AdjustedSize = a_Size + a_Alignment;
	//Go back to the buffer start if we cannot fill this allocation.
	if (m_Used + t_AdjustedSize > m_Size)
	{
		BB_ASSERT(m_Size > t_AdjustedSize,
			"Ring allocator tries to allocate something bigger then it's allocator size!");

		m_BufferPos = Pointer::Subtract(m_BufferPos, m_Used);
		m_Used = 0;
		return Alloc(a_Size, a_Alignment);
	}

	void* t_ReturnPtr = Pointer::Add(m_BufferPos, t_Adjustment);
	m_BufferPos = Pointer::Add(m_BufferPos, t_AdjustedSize);
	m_Used += t_AdjustedSize;

	return t_ReturnPtr;
}