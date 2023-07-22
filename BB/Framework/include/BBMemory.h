#pragma once
#include "Allocators/Allocators.h"
#include "Utils/Utils.h"
#include "Utils/Logger.h"

#include <type_traits>

template <typename T>
struct MacroType { typedef T type; }; //I hate C++.

namespace BB
{
	constexpr const size_t kbSize = 1024;
	constexpr const size_t mbSize = kbSize * 1024;
	constexpr const size_t gbSize = mbSize * 1024;

	//legacy code still used this, so we will just remain using this.
	using LinearAllocator_t = allocators::LinearAllocator;
	using FixedLinearAllocator_t = allocators::FixedLinearAllocator;
	using FreelistAllocator_t = allocators::FreelistAllocator;
	using POW_FreelistAllocator_t = allocators::POW_FreelistAllocator;

#define BBalloc(a_Allocator, a_Size) BB::BBalloc_f(BB_MEMORY_DEBUG_ARGS a_Allocator, a_Size, 1)
#define BBnew(a_Allocator, a_Type) new (BB::BBalloc_f(BB_MEMORY_DEBUG_ARGS a_Allocator, sizeof(a_Type), __alignof(a_Type))) a_Type
#define BBnewArr(a_Allocator, a_Length, a_Type) (BB::BBnewArr_f<MacroType<a_Type>::type>(BB_MEMORY_DEBUG_ARGS a_Allocator, a_Length))

#define BBfree(a_Allocator, a_Ptr) BBfree_f(a_Allocator, a_Ptr)
#define BBfreeArr(a_Allocator, a_Ptr) BBfreeArr_f(a_Allocator, a_Ptr)

#pragma region AllocationFunctions
	//Use the BBnew or BBalloc function instead of this.
	inline void* BBalloc_f(BB_MEMORY_DEBUG Allocator a_Allocator, const size_t a_Size, const size_t a_Alignment)
	{
		return a_Allocator.func(BB_MEMORY_DEBUG_SEND a_Allocator.allocator, a_Size, a_Alignment, nullptr);
	}

	//Use the BBnewArr function instead of this.
	template <typename T>
	inline T* BBnewArr_f(BB_MEMORY_DEBUG Allocator a_Allocator, size_t a_Length)
	{
		BB_ASSERT(a_Length != 0, "Trying to allocate an array with a length of 0.");

		if constexpr (std::is_trivially_constructible_v<T> || std::is_trivially_destructible_v<T>)
		{
			return reinterpret_cast<T*>(a_Allocator.func(BB_MEMORY_DEBUG_SEND a_Allocator.allocator, sizeof(T) * a_Length, __alignof(T), nullptr));
		}
		else
		{
			size_t t_HeaderSize;

			if constexpr (sizeof(size_t) % sizeof(T) > 0)
				t_HeaderSize = sizeof(size_t) / sizeof(T) + 1;
			else
				t_HeaderSize = sizeof(size_t) / sizeof(T);

			//Allocate the array, but shift it by sizeof(size_t) bytes forward to allow the size of the header to be put in as well.
			T* ptr = (reinterpret_cast<T*>(a_Allocator.func(BB_MEMORY_DEBUG_SEND a_Allocator.allocator, sizeof(T) * (a_Length + t_HeaderSize), __alignof(T), nullptr))) + t_HeaderSize;

			//Store the size of the array inside the first element of the pointer.
			*(reinterpret_cast<size_t*>(ptr) - 1) = a_Length;

			if constexpr (std::is_trivially_constructible_v<T>)
			{
				//Create the elements.
				for (size_t i = 0; i < a_Length; i++)
					new (&ptr[i]) T();
			}

			return ptr;
		}
	}

	template <typename T>
	inline void BBfree_f(Allocator a_Allocator, T* a_Ptr)
	{
		BB_ASSERT(a_Ptr != nullptr, "Trying to free a nullptr");
		if constexpr (!std::is_trivially_destructible_v<T>)
		{
			a_Ptr->~T();
		}
		a_Allocator.func(BB_MEMORY_DEBUG_FREE a_Allocator.allocator, 0, 0, a_Ptr);
	}

	template <typename T>
	inline void BBfreeArr_f(Allocator a_Allocator, T* a_Ptr)
	{
		BB_ASSERT(a_Ptr != nullptr, "Trying to freeArray a nullptr");

		if constexpr (std::is_trivially_constructible_v<T> || std::is_trivially_destructible_v<T>)
		{
			a_Allocator.func(BB_MEMORY_DEBUG_FREE a_Allocator.allocator, 0, 0, a_Ptr);
		}
		else
		{
			//get the array size
			size_t t_Length = *(reinterpret_cast<size_t*>(a_Ptr) - 1);

			for (size_t i = 0; i < t_Length; i++)
				a_Ptr[i].~T();

			size_t t_HeaderSize;
			if constexpr (sizeof(size_t) % sizeof(T) > 0)
				t_HeaderSize = sizeof(size_t) / sizeof(T) + 1;
			else
				t_HeaderSize = sizeof(size_t) / sizeof(T);

			a_Allocator.func(BB_MEMORY_DEBUG_FREE a_Allocator.allocator, 0, 0, a_Ptr - t_HeaderSize);
		}
	}
}
#pragma endregion // AllocationFunctions