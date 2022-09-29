#pragma once
#include <unordered_map>
#include "Allocators/Allocators.h"
#include "Utils/Utils.h"
#include "Utils/Logger.h"

namespace BB
{
	constexpr const size_t kbSize = 1024;
	constexpr const size_t mbSize = kbSize * 1024;
	constexpr const size_t gbSize = mbSize * 1024;

	void CreateMemoryDebugArena(const void* a_AllocatorAddress);
	void DestroyMemoryDebugArena(const void* a_AllocatorAddress);
	void ClearMemoryDebugArena(const void* a_AllocatorAddress);
	void AllocMemoryDebugAdjustBoundrySize(size_t& a_Size);
	void* AllocMemoryDebug(const void* a_AllocatorAddress, void* a_AllocatedPtr, const size_t a_Size, const size_t a_Alignment);
	void* FreeMemoryDebug(const void* a_AllocatorAddress, const void* a_AllocPtr);

	typedef void* (*AllocateFunc)(void* a_AllocatorData, size_t a_Size, size_t a_Alignment, void* a_OldPtr);
	struct Allocator
	{
		AllocateFunc func;
		void* allocator;
	};


	template<typename Allocator, bool debug>
	void* StandardRealloc(void* a_Allocator, size_t a_Size, size_t a_Alignment, void* a_Ptr)
	{
		if (a_Size > 0)
		{
			if constexpr (debug)
			{
				AllocMemoryDebugAdjustBoundrySize(a_Size);
			}

			void* t_AllocatedPtr = reinterpret_cast<Allocator*>(a_Allocator)->Alloc(a_Size, a_Alignment);

			if constexpr (debug)
			{
				t_AllocatedPtr = AllocMemoryDebug(a_Allocator,
					t_AllocatedPtr,
					a_Size,
					a_Alignment);
			}
			return t_AllocatedPtr;
		}
		else
		{
			if constexpr (debug)
			{
				a_Ptr = FreeMemoryDebug(a_Allocator, a_Ptr);
			}
			reinterpret_cast<Allocator*>(a_Allocator)->Free(a_Ptr);
			return nullptr;
		}
	}

	template<typename AllocatorType, bool debug>
	struct AllocatorTemplate
	{
		operator Allocator()
		{
			Allocator t_AllocatorInterface;
			t_AllocatorInterface.allocator = this;
			t_AllocatorInterface.func = StandardRealloc<AllocatorType, debug>;
			return t_AllocatorInterface;
		}

		AllocatorTemplate(size_t a_AllocatorSize)
			: allocator(a_AllocatorSize) 
		{
			if constexpr (debug)
			{
				CreateMemoryDebugArena(this);
			}
		}

		~AllocatorTemplate()
		{
			if constexpr (debug)
			{
				DestroyMemoryDebugArena(this);
			}
		}

		void Clear()
		{
			allocator.Clear();
			ClearMemoryDebugArena(this);
		}

		AllocatorType allocator;
	};
#ifdef _DEBUG
	using LinearAllocator_t = AllocatorTemplate<allocators::LinearAllocator, true>;
	using FixedLinearAllocator_t = AllocatorTemplate<allocators::FixedLinearAllocator, true>;
	using FreelistAllocator_t = AllocatorTemplate<allocators::FreelistAllocator, true>;
	using POW_FreelistAllocator_t = AllocatorTemplate<allocators::POW_FreelistAllocator, true>;
#else
	using LinearAllocator_t = AllocatorTemplate<allocators::LinearAllocator, false>;
	using FixedLinearAllocator_t = AllocatorTemplate<allocators::FixedLinearAllocator, false>;
	using FreelistAllocator_t = AllocatorTemplate<allocators::FreeListAllocator, false>;
	using POW_FreelistAllocator_t = AllocatorTemplate<allocators::POW_FreeListAllocator, false>;
#endif //_DEBUG


#pragma region AllocationFunctions
	inline void* BBalloc(Allocator a_Allocator, const size_t a_Size)
	{
		return a_Allocator.func(a_Allocator.allocator, a_Size, 1, nullptr);
	}

	template <typename T>
	inline T* BBnew(Allocator a_Allocator)
	{
		return new (reinterpret_cast<T*>(a_Allocator.func(a_Allocator.allocator, sizeof(T), __alignof(T), nullptr))) T();
	}

	template <typename T>
	inline T* BBnew(Allocator a_Allocator, const T& a_T)
	{
		return new (reinterpret_cast<T*>(a_Allocator.func(a_Allocator.allocator, sizeof(T), __alignof(T), nullptr))) T(a_T);
	}

	template <typename T, typename... Args>
	inline T* BBnew(Allocator a_Allocator, Args&&... a_Args)
	{
		return new (reinterpret_cast<T*>(a_Allocator.func(a_Allocator.allocator, sizeof(T), __alignof(T), nullptr))) T(std::forward<Args>(a_Args)...);
	}

	template <typename T>
	inline T* BBnewArr(Allocator a_Allocator, size_t a_Length)
	{
		BB_ASSERT(a_Length != 0, "Trying to allocate an array with a length of 0.");

		if constexpr (std::is_trivially_constructible_v<T> || std::is_trivially_destructible_v<T>)
		{
			return reinterpret_cast<T*>(a_Allocator.func(a_Allocator.allocator, sizeof(T) * a_Length, __alignof(T), nullptr));
		}
		else
		{
			size_t t_HeaderSize;

			if constexpr (sizeof(size_t) % sizeof(T) > 0)
				t_HeaderSize = sizeof(size_t) / sizeof(T) + 1;
			else
				t_HeaderSize = sizeof(size_t) / sizeof(T);

			//Allocate the array, but shift it by sizeof(size_t) bytes forward to allow the size of the header to be put in as well.
			T* ptr = (reinterpret_cast<T*>(a_Allocator.func(a_Allocator.allocator, sizeof(T) * (a_Length + t_HeaderSize), __alignof(T), nullptr))) + t_HeaderSize;

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
	inline void BBfree(Allocator a_Allocator, T* a_Ptr)
	{
		BB_ASSERT(a_Ptr != nullptr, "Trying to free a nullptr");
		if constexpr (!std::is_trivially_destructible_v<T>)
		{
			a_Ptr->~T();
		}
		a_Allocator.func(a_Allocator.allocator, 0, 0, a_Ptr);
	}

	template <typename T>
	inline void BBfreeArr(Allocator a_Allocator, T* a_Ptr)
	{
		BB_ASSERT(a_Ptr != nullptr, "Trying to freeArray a nullptr");

		if constexpr (std::is_trivially_constructible_v<T> || std::is_trivially_destructible_v<T>)
		{
			a_Allocator.func(a_Allocator.allocator, 0, 0, a_Ptr);
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

			a_Allocator.func(a_Allocator.allocator, 0, 0, a_Ptr - t_HeaderSize);
		}
	}
}
#pragma endregion // AllocationFunctions