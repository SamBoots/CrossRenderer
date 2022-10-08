#pragma once
#include <unordered_map>
#include "Allocators/Allocators.h"
#include "Utils/Utils.h"
#include "Utils/Logger.h"

#include <iostream>


template <typename T>
struct MacroType { typedef T type; }; //I hate C++.

namespace BB
{
#ifdef _DEBUG
#define BB_MEMORY_DEBUG const char* a_File, size_t a_Line,
#define BB_MEMORY_DEBUG_ARGS __FILE__, __LINE__,
#define BB_MEMORY_DEBUG_SEND a_File, a_Line,
#define BB_MEMORY_DEBUG_FREE nullptr, 0,
#else
#define BB_MEMORY_DEBUG 
#define BB_MEMORY_DEBUG_ARGS
#define BB_MEMORY_DEBUG_SEND
#define BB_MEMORY_DEBUG_FREE
#endif //_DEBUG

	constexpr const size_t MEMORY_BOUNDRY_FRONT = sizeof(size_t);
	constexpr const size_t MEMORY_BOUNDRY_BACK = sizeof(size_t);

	constexpr const size_t kbSize = 1024;
	constexpr const size_t mbSize = kbSize * 1024;
	constexpr const size_t gbSize = mbSize * 1024;

	//Checks Adds memory boundry to an allocation log.
	void* Memory_AddBoundries(void* a_Front, size_t a_AllocSize);
	//Checks the memory boundries, 
	void Memory_CheckBoundries(void* a_Front, void* a_Back);

	struct AllocationLog
	{
		AllocationLog* prev;
		void* front;
		void* back;
		size_t allocSize;
		const char* file;
		size_t line;
	};

	struct AllocationLogger
	{
		AllocationLog* front;
	};

	static AllocationLog* DeleteEntry(AllocationLog* a_Front, const AllocationLog* a_DeletedEntry)
	{
		AllocationLog* t_Entry = a_Front;
		while (t_Entry->prev != a_DeletedEntry)
		{
			t_Entry = t_Entry->prev;
		}
		t_Entry->prev = a_DeletedEntry->prev;

		return a_Front;
	}

	typedef void* (*AllocateFunc)(BB_MEMORY_DEBUG void* a_AllocatorData, size_t a_Size, size_t a_Alignment, void* a_OldPtr);
	struct Allocator
	{
		AllocateFunc func;
		void* allocator;
	};

	template<typename Allocator_t>
	void* StandardRealloc(BB_MEMORY_DEBUG void* a_Allocator, size_t a_Size, size_t a_Alignment, void* a_Ptr)
	{
		if (a_Size > 0)
		{
#ifdef _DEBUG
			a_Size += MEMORY_BOUNDRY_FRONT + MEMORY_BOUNDRY_BACK + sizeof(AllocationLog);
#endif //_DEBUG
			void* t_AllocatedPtr = reinterpret_cast<Allocator_t*>(a_Allocator)->Alloc(a_Size, a_Alignment);
#ifdef _DEBUG
			//Get the space for the allocation log, but keep enough space for the boundry check.
			AllocationLog* t_AllocLog = reinterpret_cast<AllocationLog*>(
				Pointer::Add(t_AllocatedPtr, MEMORY_BOUNDRY_FRONT));

			t_AllocLog->prev = reinterpret_cast<Allocator_t*>(a_Allocator)->frontLog;
			t_AllocLog->front = t_AllocatedPtr;
			t_AllocLog->back = Memory_AddBoundries(t_AllocatedPtr, a_Size);
			t_AllocLog->allocSize = a_Size;
			t_AllocLog->file = a_File;
			t_AllocLog->line = a_Line;
			reinterpret_cast<Allocator_t*>(a_Allocator)->frontLog = t_AllocLog;
			t_AllocatedPtr = Pointer::Add(t_AllocatedPtr, MEMORY_BOUNDRY_FRONT + sizeof(AllocationLog));
#endif //_DEBUG
			return t_AllocatedPtr;
		}
		else
		{
#ifdef _DEBUG
			AllocationLog* t_AllocLog = reinterpret_cast<AllocationLog*>(
				Pointer::Subtract(a_Ptr, sizeof(AllocationLog)));

			Memory_CheckBoundries(t_AllocLog->front, t_AllocLog->back);
			a_Ptr = Pointer::Subtract(a_Ptr, MEMORY_BOUNDRY_FRONT + sizeof(AllocationLog));

			AllocationLog* t_FrontLog = reinterpret_cast<Allocator_t*>(a_Allocator)->frontLog;

			if (t_AllocLog != reinterpret_cast<Allocator_t*>(a_Allocator)->frontLog)
				DeleteEntry(t_FrontLog, t_AllocLog);
			else
				reinterpret_cast<Allocator_t*>(a_Allocator)->frontLog = t_FrontLog->prev;
#endif //_DEBUG
			reinterpret_cast<Allocator_t*>(a_Allocator)->Free(a_Ptr);
			return nullptr;
		}
	}

	template<typename AllocatorType>
	struct AllocatorTemplate
	{
		operator Allocator()
		{
			Allocator t_AllocatorInterface;
			t_AllocatorInterface.allocator = this;
			t_AllocatorInterface.func = StandardRealloc<AllocatorTemplate<AllocatorType>>;
			return t_AllocatorInterface;
		}

		AllocatorTemplate(size_t a_AllocatorSize)
			: allocator(a_AllocatorSize) {};

		~AllocatorTemplate()
		{
#ifdef _DEBUG
			AllocationLog* t_FrontLog = frontLog;
			while (t_FrontLog != nullptr)
			{
				std::cout << "Memory leak accured in file: " << t_FrontLog->file << "\n on line: " 
					<< t_FrontLog->line << "\n leak size: " << t_FrontLog->allocSize << "\n";
				t_FrontLog = t_FrontLog->prev;
			}
#endif //_DEBUG
			Clear();
		}

		void* Alloc(size_t a_Size, size_t a_Alignment)
		{
			return allocator.Alloc(a_Size, a_Alignment);
		}

		void Free(void* a_Ptr)
		{
			allocator.Free(a_Ptr);
		}

		void Clear()
		{
#ifdef _DEBUG
			while (frontLog != nullptr)
			{
				Memory_CheckBoundries(frontLog->front, frontLog->back);
				frontLog = frontLog->prev;
			}
#endif //_DEBUG
			allocator.Clear();
		}

		AllocatorType allocator;
#ifdef _DEBUG
		AllocationLog* frontLog = nullptr;
#endif //_DEBUG
	};

	using LinearAllocator_t = AllocatorTemplate<allocators::LinearAllocator>;
	using FixedLinearAllocator_t = AllocatorTemplate<allocators::FixedLinearAllocator>;
	using FreelistAllocator_t = AllocatorTemplate<allocators::FreelistAllocator>;
	using POW_FreelistAllocator_t = AllocatorTemplate<allocators::POW_FreelistAllocator>;

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