#include "Utils/Logger.h"
#include "BackingAllocator.h"
#include "Utils/Utils.h"
#include "OS/OSDevice.h"

#include <sys/mman.h>

using namespace BB;

struct VirtualHeader
{
	size_t bytesCommited;
	size_t bytesReserved;
};

void* BB::mallocVirtual(void* a_Start, size_t& a_Size, const virtual_reserve_extra a_ReserveSize)
{
	//Adjust the requested bytes by the page size and the minimum virtual allocaion size.
	size_t t_PageAdjustedSize = Math::RoundUp(a_Size + sizeof(VirtualHeader), AppOSDevice().VirtualMemoryPageSize());
	t_PageAdjustedSize = Math::Max(t_PageAdjustedSize, AppOSDevice().VirtualMemoryMinimumAllocation());

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
			mprotect(t_PageHeader, t_PageHeader->bytesCommited, PROT_READ | PROT_WRITE);
			BB_ASSERT(AppOSDevice().LatestOSError() == 0x0, "Linux API error mprotect.");
			return t_NewCommitRange;
		}

		BB_ASSERT(false, "Going over reserved memory! Make sure to reserve more memory")
	}

	//When making a new header reserve a lot more then that is requested to support later resizes better.
	size_t t_AdditionalReserve = t_PageAdjustedSize * static_cast<size_t>(a_ReserveSize);
	//The prot is PROT_NONE so that it cannot be accessed, this will reflect reserving memory like the VirtualAlloc call from windows.
	void* t_Address = mmap(a_Start, t_AdditionalReserve, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);
	BB_ASSERT(AppOSDevice().LatestOSError() == 0x0, "Linux API error mmap.");

	//Now commit enough memory that the user requested.
	//Instead of VirtualAlloc and commiting memory we will just remove the protection.
	mprotect(t_Address, t_PageAdjustedSize, PROT_READ | PROT_WRITE);
	BB_ASSERT(AppOSDevice().LatestOSError() == 0x0, "Linux API error mprotect.");

	//Set the header of the allocator, used for later resizes and when you need to free it.
	reinterpret_cast<VirtualHeader*>(t_Address)->bytesCommited = t_PageAdjustedSize;
	reinterpret_cast<VirtualHeader*>(t_Address)->bytesReserved = t_AdditionalReserve;

	//Return the pointer that does not include the StartPageHeader
	return Pointer::Add(t_Address, sizeof(VirtualHeader));
}

void BB::freeVirtual(void* a_Ptr)
{
	VirtualHeader* t_Header = reinterpret_cast<VirtualHeader*>(Pointer::Subtract(a_Ptr, sizeof(VirtualHeader)));
	munmap(t_Header, t_Header->bytesReserved);
	BB_ASSERT(AppOSDevice().LatestOSError() == 0x0, "Linux API error munmap.");
}