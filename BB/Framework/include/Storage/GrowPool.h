#pragma once
#include "Utils/Logger.h"
#include "Allocators/BackingAllocator.h"

namespace BB
{
	/// <summary>
	/// ONLY USE THIS FOR BIG ALLOCATIONS. It uses virtual alloc to extend the pool.
	/// NON_RAII and does not support POD types. 
	/// You must call the destructor and constructor yourself.
	/// Create the pool by using CreatePool and DestroyPool. 
	/// A special container that is not responsible for it's own deallocation. 
	/// It uses virtual alloc in the background.
	/// </summary>
	template<typename T>
	class GrowPool
	{
	public:
		GrowPool() {};
#ifdef _DEBUG
		~GrowPool();
#endif _DEBUG

		//just delete these for safety, copies might cause errors.
		GrowPool(const GrowPool&) = delete;
		GrowPool(const GrowPool&&) = delete;
		GrowPool& operator =(const GrowPool&) = delete;
		GrowPool& operator =(GrowPool&&) = delete;

		/// <summary>
		/// Create a pool that can hold members equal to a_Size.
		/// Will likely over allocate more due to how virtual memory paging works.
		/// </summary>
		void CreatePool(const size_t a_Size);
		void DestroyPool();

		/// <summary>
		/// Get an object from the pool, returns nullptr if the pool is empty.
		/// </summary>
		T* Get();
		/// <summary>
		/// Return an object to the pool.
		/// </summary>
		void Free(T* a_Ptr);

	private:
#ifdef _DEBUG
		//Debug we can check it's current size.
		size_t m_Size = 0;
		size_t m_Capacity = 0;
#endif // _DEBUG

		void* m_Start = nullptr;
		T** m_Pool = nullptr;
	};

#ifdef _DEBUG
	template<typename T>
	inline GrowPool<T>::~GrowPool()
	{
		BB_ASSERT(m_Start == nullptr, "Memory pool was not destroyed before it went out of scope!");
	}
#endif _DEBUG

	template<typename T>
	inline void GrowPool<T>::CreatePool(const size_t a_Size)
	{
		BB_STATIC_ASSERT(sizeof(T) >= sizeof(void*), "Pool object is smaller then the size of a pointer.");
		BB_ASSERT(m_Start == nullptr, "Trying to create a pool while one already exists!");

		size_t t_AllocSize = a_Size * sizeof(T);
		m_Start = mallocVirtual(m_Start, t_AllocSize);
		m_Pool = reinterpret_cast<T**>(m_Start);
		const size_t t_SpaceForElements = t_AllocSize / sizeof(T);

#ifdef _DEBUG
		m_Size = 0;
		m_Capacity = t_SpaceForElements;
#endif //_DEBUG

		T** t_Pool = m_Pool;

		for (size_t i = 0; i < t_SpaceForElements - 1; i++)
		{
			*t_Pool = (reinterpret_cast<T*>(t_Pool)) + 1;
			t_Pool = reinterpret_cast<T**>(*t_Pool);
		}
		*t_Pool = nullptr;
	}

	template<typename T>
	inline void GrowPool<T>::DestroyPool()
	{
		freeVirtual(m_Start);
#ifdef _DEBUG
		//Set everything to 0 in debug to indicate it was destroyed.
		memset(this, 0, sizeof(BB::GrowPool<T>));
#endif //_DEBUG
		m_Start = nullptr;
	}

	template<class T>
	inline T* GrowPool<T>::Get()
	{
		if (*m_Pool == nullptr)
		{
			BB_WARNING(false, "Growing the growpool, if this happens often try to reserve more.", WarningType::OPTIMALIZATION);
			//get more memory!
			size_t t_AllocSize = 0;
			mallocVirtual(m_Start, t_AllocSize);
			const size_t t_SpaceForElements = t_AllocSize / sizeof(T);

#ifdef _DEBUG
			m_Capacity += t_SpaceForElements;
#endif //_DEBUG

			T** t_Pool = m_Pool;

			for (size_t i = 0; i < t_SpaceForElements - 1; i++)
			{
				*t_Pool = (reinterpret_cast<T*>(t_Pool)) + 1;
				t_Pool = reinterpret_cast<T**>(*t_Pool);
			}
			*t_Pool = nullptr;
		}

		//Take the freelist
		T* t_Ptr = reinterpret_cast<T*>(m_Pool);
		//Set the new head of the freelist.
		m_Pool = reinterpret_cast<T**>(*m_Pool);

#ifdef _DEBUG
		++m_Size;
#endif //_DEBUG

		return t_Ptr;
	}

	template<typename T>
	inline void GrowPool<T>::Free(T* a_Ptr)
	{
#ifdef _DEBUG
		BB_ASSERT((a_Ptr >= m_Start && a_Ptr < Pointer::Add(m_Start, m_Capacity * sizeof(T))), "Trying to free an pool object that is not part of this pool!");
		--m_Size;
#endif // _DEBUG

		//Set the previous free list to the new head.
		(*reinterpret_cast<T**>(a_Ptr)) = reinterpret_cast<T*>(m_Pool);
		//Set the new head.
		m_Pool = reinterpret_cast<T**>(a_Ptr);
	}
}