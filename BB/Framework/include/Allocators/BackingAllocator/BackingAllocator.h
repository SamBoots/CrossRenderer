#pragma once
#include <cstdlib>

namespace BB
{
	enum class virtual_reserve_extra : size_t
	{
		none = 1, //Don't reserve more virtual space, use this if you will never resize a mallocVirtual address.
#ifdef _64BIT
		half = 64, //reserve 64 times more virtual space (16 times more on x86).
		standard = 128, //reserve 128 times more virtual space (32 times more on x86).
		extra = 256  //reserve 256 times more virtual space (64 times more on x86).
#endif //_X64
#ifdef _32BIT
		half = 16, //reserve 64 times more virtual space (16 times more on x86).
		standard = 32, //reserve 128 times more virtual space (32 times more on x86).
		extra = 64  //reserve 256 times more virtual space (64 times more on x86).
#endif //_X86
	};

	/// <summary>
	/// Reserve and commit virtual memory at the same time. 
	/// </summary>
	/// <param name="a_Start:"> The previous pointer used to commit the backing memory, nullptr if this is the first instance of allocation. </param>
	/// <param name="a_Size:"> size of the virtual memory allocation in bytes, will be changed to be above OSDevice.virtual_memory_minimum_allocation and a multiple of OSDevice.virtual_memory_page_size. If a_Start is not a nullptr it will extend the commited range, will also be changed similiarly to normal.</param>
	/// <param name="a_ReserveSize:"> How much extra memory is reserved for possible resizes. Default is virtual_reserve_extra::standard, which will reserve 128 times more virtual space (64 times more on x86).</param>
	/// <returns>Pointer to the start of the virtual memory, or the updated commited range. </returns>
	void* mallocVirtual(void* a_Start, size_t& a_Size, const virtual_reserve_extra a_ReserveSize = virtual_reserve_extra::standard);
	
	/// <summary>
	/// Free all the pages from a given pointer.
	/// </summary>
	/// /// <param name="a_Ptr:"> The pointer returned from mallocVirtual when you provided a nullptr to a_Start. </param>
	void freeVirtual(void* a_Ptr);
}