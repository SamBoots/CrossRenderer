#pragma once

#include "MemoryArena.h"
#include "Allocators.h"

namespace BB
{
	constexpr const size_t kbSize = 1024;
	constexpr const size_t mbSize = kbSize * 1024;
	constexpr const size_t gbSize = mbSize * 1024;

#ifdef _DEBUG
	//Default types
	using LinearAllocator_t = BB::MemoryArena<BB::allocators::LinearAllocator, BB::ThreadPolicy::Single_Thread, true>;
	using FreeListAllocator_t = BB::MemoryArena<BB::allocators::FreelistAllocator, BB::ThreadPolicy::Single_Thread, true>;
	using POW_FreeListAllocator_t = BB::MemoryArena<BB::allocators::POW_FreelistAllocator, BB::ThreadPolicy::Single_Thread, true>;
#else
	//Default types
	using LinearAllocator_t = BB::MemoryArena<BB::allocators::LinearAllocator, BB::ThreadPolicy::Single_Thread, false>;
	using FreeListAllocator_t = BB::MemoryArena<BB::allocators::FreelistAllocator, BB::ThreadPolicy::Single_Thread, false>;
	using POW_FreeListAllocator_t = BB::MemoryArena<BB::allocators::POW_FreelistAllocator, BB::ThreadPolicy::Single_Thread, false>;
#endif

#pragma region AllocationFunctions
	inline void* BBalloc(Allocator a_Arena, const size_t a_Size)
	{
		return a_Arena.func(a_Arena.allocator, a_Size, 1, nullptr);
	}

	template <typename T>
	inline T* BBnew(Allocator a_Arena)
	{
		return new (reinterpret_cast<T*>(a_Arena.func(a_Arena.allocator, sizeof(T), __alignof(T), nullptr))) T();
	}

	template <typename T>
	inline T* BBnew(Allocator a_Arena, const T& a_T)
	{
		return new (reinterpret_cast<T*>(a_Arena.func(a_Arena.allocator, sizeof(T), __alignof(T), nullptr))) T(a_T);
	}

	template <typename T, typename... Args>
	inline T* BBnew(Allocator a_Arena, Args&&... a_Args)
	{
		return new (reinterpret_cast<T*>(a_Arena.func(a_Arena.allocator, sizeof(T), __alignof(T), nullptr))) T(std::forward<Args>(a_Args)...);
	}

	template <typename T>
	inline T* BBnewArr(Allocator a_Arena, size_t a_Length)
	{
		BB_ASSERT(a_Length != 0, "Trying to allocate an array with a length of 0.");

		if constexpr (std::is_trivially_constructible_v<T> || std::is_trivially_destructible_v<T>)
		{
			return reinterpret_cast<T*>(a_Arena.func(a_Arena.allocator, sizeof(T) * a_Length, __alignof(T), nullptr));
		}
		else
		{
			size_t t_HeaderSize;

			if constexpr (sizeof(size_t) % sizeof(T) > 0)
				t_HeaderSize = sizeof(size_t) / sizeof(T) + 1;
			else
				t_HeaderSize = sizeof(size_t) / sizeof(T);

			//Allocate the array, but shift it by sizeof(size_t) bytes forward to allow the size of the header to be put in as well.
			T* ptr = (reinterpret_cast<T*>(a_Arena.func(a_Arena.allocator, sizeof(T) * (a_Length + t_HeaderSize), __alignof(T), nullptr))) + t_HeaderSize;

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
	inline void BBfree(Allocator a_Arena, T* a_Ptr)
	{
		BB_ASSERT(a_Ptr != nullptr, "Trying to free a nullptr");
		if constexpr (std::is_trivially_destructible_v<T>)
		{
			a_Ptr->~T();
		}
		a_Arena.func(a_Arena.allocator, 0, 0, a_Ptr);
	}

	template <typename T>
	inline void BBfreeArr(Allocator a_Arena, T* a_Ptr)
	{
		BB_ASSERT(a_Ptr != nullptr, "Trying to freeArray a nullptr");

		if constexpr (std::is_trivially_constructible_v<T> || std::is_trivially_destructible_v<T>)
		{
			a_Arena.func(a_Arena.allocator, 0, 0, a_Ptr);
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

			a_Arena.func(a_Arena.allocator, 0, 0, a_Ptr - t_HeaderSize);
		}
	}
#pragma endregion // AllocationFunctions

#pragma region Debug

#ifdef _64BIT
	//A global debug allocator.
	static FreeListAllocator_t DebugAllocator{ mbSize * 8 };
#endif //_64BIT
#ifdef _32BIT
	//16mb of debug size, more can be reserved automatically.
	//A global debug allocator.
	static FreeListAllocator_t DebugAllocator{ mbSize * 8 };
#endif //_32BIT

	//Global alloc that uses a debug allocator. Only use this when debugging or testing features. It will pop warnings in release mode.
	template <typename T, typename... Args>
	T* BBglobalnew(Args&&... a_Args)
	{
#ifndef _DEBUG
		BB_WARNING(false, "BBglobalalloc used while in release mode. This should only happen if you are testing unfinished features. Consinder using a temporary or system allocator instead.", WarningType::OPTIMALIZATION);
#endif //_DEBUG
		return BBnew<T, Args...>(DebugAllocator, a_Args...);
	}

	//Global alloc that uses a debug allocator. Only use this when debugging or testing features. It will pop warnings in release mode.
	template <typename T, typename... Args>
	T* BBglobalnewArr(size_t a_Count)
	{
#ifndef _DEBUG
		BB_WARNING(false, "BBglobalalloc used while in release mode. This should only happen if you are testing unfinished features. Consinder using a temporary or system allocator instead.", WarningType::OPTIMALIZATION);
#endif //_DEBUG
		return BBnewArr<T>(DebugAllocator, a_Count);
	}

	//Global destroy that uses a debug allocator. Only use this when debugging or testing features. It will pop warnings in release mode.
	template <typename T>
	void BBglobalfree(T* a_Ptr)
	{
		BBfree<T>(DebugAllocator, a_Ptr);
	}

	//Global destroy that uses a debug allocator. Only use this when debugging or testing features. It will pop warnings in release mode.
	template <typename T>
	void BBglobalfreeArr(T* a_Ptr)
	{
		BBfreeArr<T>(DebugAllocator, a_Ptr);
	}

#pragma endregion // Debug
}