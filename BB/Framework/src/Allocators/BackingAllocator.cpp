#include "Utils/Logger.h"
#include "BackingAllocator.h"
#include "Utils/Utils.h"
#include "OS/Program.h"
#include "Math.inl"

using namespace BB;

struct VirtualHeader
{
	size_t bytesCommited;
	size_t bytesReserved;
};

void* BB::mallocVirtual(void* a_Start, size_t& a_Size, const size_t a_ReserveSize)
{
	//Adjust the requested bytes by the page size and the minimum virtual allocaion size.
	size_t t_PageAdjustedSize = Math::RoundUp(a_Size + sizeof(VirtualHeader), VirtualMemoryPageSize());
	t_PageAdjustedSize = Max(t_PageAdjustedSize, VirtualMemoryMinimumAllocation());

	//Set the reference of a_Size so that the allocator has enough memory until the end of the page.
	a_Size = t_PageAdjustedSize - sizeof(VirtualHeader);

	//Check the pageHeader
	if (a_Start != nullptr)
	{
		//Get the header for preperation to resize it.
		VirtualHeader* t_PageHeader = reinterpret_cast<VirtualHeader*>(Pointer::Subtract(a_Start, sizeof(VirtualHeader)));

		//Commit more memory if there is enough reserved.
		if (t_PageHeader->bytesReserved > t_PageAdjustedSize + t_PageHeader->bytesCommited)
		{
			void* t_NewCommitRange = Pointer::Add(t_PageHeader, t_PageHeader->bytesCommited);

			t_PageHeader->bytesCommited += t_PageAdjustedSize;
			BB_ASSERT(CommitVirtualMemory(t_PageHeader, t_PageHeader->bytesCommited) != 0, "Error commiting virtual memory");
			return t_NewCommitRange;
		}

		BB_ASSERT(false, "Going over reserved memory! Make sure to reserve more memory");
	}

	//When making a new header reserve a lot more then that is requested to support later resizes better.
	size_t t_AdditionalReserve = t_PageAdjustedSize * a_ReserveSize;
	void* t_Address = ReserveVirtualMemory(t_AdditionalReserve);
	BB_ASSERT(t_Address != NULL, "Error reserving virtual memory");

	//Now commit enough memory that the user requested.
	BB_ASSERT(CommitVirtualMemory(t_Address, t_PageAdjustedSize) != NULL, "Error commiting right after a reserve virtual memory");

	//Set the header of the allocator, used for later resizes and when you need to free it.
	reinterpret_cast<VirtualHeader*>(t_Address)->bytesCommited = t_PageAdjustedSize;
	reinterpret_cast<VirtualHeader*>(t_Address)->bytesReserved = t_AdditionalReserve;

	//Return the pointer that does not include the StartPageHeader
	return Pointer::Add(t_Address, sizeof(VirtualHeader));
}

void BB::freeVirtual(void* a_Ptr)
{
	BB_ASSERT(ReleaseVirtualMemory(Pointer::Subtract(a_Ptr, sizeof(VirtualHeader))) != 0, "Error on releasing virtual memory");
}

//#pragma region Unit Test
//
//#pragma warning (push, 0)
//#include <gtest/gtest.h>
//#pragma warning (pop)
//
//struct MockAllocator
//{
//	MockAllocator(size_t a_Size)
//	{
//		start = reinterpret_cast<uint8_t*>(mallocVirtual(start, a_Size));
//		maxSize = a_Size;
//		//using memset because the memory is NOT commited to ram unless it's accessed.
//		memset(start, 5215, a_Size);
//		buffer = start;
//	}
//
//	~MockAllocator()
//	{
//		freeVirtual(start);
//	}
//
//	void* Alloc(size_t a_Size)
//	{
//		void* t_Address = buffer;
//		currentSize += a_Size;
//
//		if (currentSize > maxSize)
//		{
//			size_t t_BufferIncrease{};
//			if (maxSize > a_Size)
//				t_BufferIncrease = Math::RoundUp(a_Size, maxSize);
//			else
//				t_BufferIncrease = a_Size;
//
//			mallocVirtual(start, t_BufferIncrease);
//			//using memset because the memory is NOT commited to ram unless it's accessed.
//			maxSize += t_BufferIncrease;
//		}
//		memset(buffer, 16, a_Size);
//		buffer = Pointer::Add(buffer, a_Size);
//		return t_Address;
//	};
//
//	const size_t SpaceLeft() const
//	{
//		return maxSize - currentSize;
//	}
//	//Not supporting free yet.
//	//void free(size_t a_Size);
//
//	size_t maxSize;
//	size_t currentSize = 0;
//	void* start = nullptr;
//	void* buffer;
//};
//
//TEST(MemoryAllocators_Backend_Windows, COMMIT_RESERVE_PAGES)
//{
//	//Allocator size is equal to half a page, it will allocate an entire page in the background anyway.
//	MockAllocator t_Allocator(AppOSDevice().VirtualMemoryMinimumAllocation());
//	ASSERT_EQ(AppOSDevice().LatestOSError(), 0x0) << "Windows API error on creating the MockAllocator.";
//
//	VirtualHeader lastHeader = *reinterpret_cast<VirtualHeader*>(Pointer::Subtract(t_Allocator.start, sizeof(VirtualHeader)));
//	VirtualHeader newHeader;
//
//	//Allocate memory equal to an entire page, this should increase the commited amount of pages, but not reserved.
//	t_Allocator.Alloc(AppOSDevice().VirtualMemoryMinimumAllocation() * 3);
//	ASSERT_EQ(AppOSDevice().LatestOSError(), 0x0) << "Windows API error on commiting more memory.";
//	newHeader = *reinterpret_cast<VirtualHeader*>(Pointer::Subtract(t_Allocator.start, sizeof(VirtualHeader)));
//	EXPECT_NE(lastHeader.bytesCommited, newHeader.bytesCommited) << "Bytes commited is not changed, while it should change!";
//	//Reserved is now never changed.
//	//EXPECT_NE(lastHeader.bytes_reserved, newHeader.bytes_reserved) << "Bytes reserved is not changed, while it should change!";
//	lastHeader = newHeader;
//}
//#pragma endregion