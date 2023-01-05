#pragma once
#include "Utils/Logger.h"
#include "BBMemory.h"

namespace BB
{
	/// <summary>
	/// NON_RAII and does not support non-POD types. 
	/// You must call the destructor and constructor yourself.
	/// Create the pool by using CreatePool and DestroyPool. 
	/// A special container that is not responsible for it's own deallocation. 
	/// It has enough memory to hold element T equal to the size given. 
	/// </summary>
	template<typename T>
	class Pool
	{
	public:
#ifdef _DEBUG
		//Must have a constructor because of the debug destructor
		Pool() {};
		~Pool();
#endif _DEBUG

		//We do no copying
		Pool(const Pool&) = delete;
		Pool& operator =(const Pool&) = delete;

		//We do moving however
		Pool(Pool&& a_Pool);
		Pool& operator =(Pool&& a_Rhs);

		void CreatePool(Allocator a_Allocator, const size_t a_Size);
		void DestroyPool(Allocator a_Allocator);

		/// <summary>
		/// Get an object from the pool, returns nullptr if the pool is empty.
		/// </summary>
		T* Get();
		/// <summary>
		/// Return an object to the pool.
		/// </summary>
		void Free(T* a_Ptr);

		T* data() const { return reinterpret_cast<T*>(m_Start); }

	private:
#ifdef _DEBUG
		//Debug we can check it's current size.
		size_t m_Size;
 		size_t m_Capacity;
		//Check if we use the same allocator for removal.
		Allocator m_Allocator{};
#endif // _DEBUG

		void* m_Start = nullptr;
		T** m_Pool;
	};

#ifdef _DEBUG
	template<typename T>
	inline Pool<T>::~Pool()
	{
		BB_ASSERT(m_Start == nullptr, "Memory pool was not destroyed before it went out of scope!");
	}
#endif // _DEBUG

	template<typename T>
	inline Pool<T>::Pool(Pool&& a_Pool)
	{
		m_Start = a_Pool.m_Start;
		m_Pool = a_Pool.m_Pool;
		m_Start = nullptr;
		m_Pool = nullptr;

#ifdef _DEBUG
		m_Size = a_Pool.m_Size;
		m_Capacity = a_Pool.m_Capacity;
		m_Allocator = a_Pool.m_Allocator;
		a_Pool.m_Size = 0;
		a_Pool.m_Capacity = 0;
		a_Pool.m_Allocator.allocator = nullptr;
		a_Pool.m_Allocator.func = nullptr;
#endif // _DEBUG
	}

	template<typename T>
	inline Pool<T>& Pool<T>::operator=(Pool&& a_Rhs)
	{
		m_Start = a_Rhs.m_Start;
		m_Pool = a_Rhs.m_Pool;
		a_Rhs.m_Start = nullptr;
		a_Rhs.m_Pool = nullptr;

#ifdef _DEBUG
		m_Size = a_Rhs.m_Size;
		m_Capacity = a_Rhs.m_Capacity;
		m_Allocator = a_Rhs.m_Allocator;
		a_Rhs.m_Size = 0;
		a_Rhs.m_Capacity = 0;
		a_Rhs.m_Allocator.allocator = nullptr;
		a_Rhs.m_Allocator.func = nullptr;
#endif // _DEBUG

		return *this;
	}

	template<typename T>
	inline void Pool<T>::CreatePool(Allocator a_Allocator, const size_t a_Size)
	{
		BB_STATIC_ASSERT(sizeof(T) >= sizeof(void*), "Pool object is smaller then the size of a pointer.");
		BB_ASSERT(m_Start == nullptr, "Trying to create a pool while one already exists!");

#ifdef _DEBUG
		m_Size = 0;
		m_Capacity = a_Size;
		m_Allocator = a_Allocator;
#endif //_DEBUG

		m_Start = BBalloc(a_Allocator, a_Size * sizeof(T));
		m_Pool = reinterpret_cast<T**>(m_Start);

		T** t_Pool = m_Pool;

		for (size_t i = 0; i < a_Size - 1; i++)
		{
			*t_Pool = (reinterpret_cast<T*>(t_Pool)) + 1;
			t_Pool = reinterpret_cast<T**>(*t_Pool);
		}
		*t_Pool = nullptr;
	}

	template<typename T>
	inline void Pool<T>::DestroyPool(Allocator a_Allocator)
	{
#ifdef _DEBUG
		BB_ASSERT(m_Allocator.allocator == a_Allocator.allocator, "Trying to delete a pool with an allocator that wasn't used in it's CreatePool function.");
#endif //_DEBUG
		BBfree(a_Allocator, m_Start);
#ifdef _DEBUG
		//Set everything to 0 in debug to indicate it was destroyed.
		memset(this, 0, sizeof(BB::Pool<T>));
#endif //_DEBUG
		m_Start = nullptr;
	}

	template<class T>
	inline T* Pool<T>::Get()
	{
		if (m_Pool == nullptr)
		{
			BB_WARNING(false, "Trying to get an pool object while there are none left!", WarningType::HIGH);
			return nullptr;
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
	inline void BB::Pool<T>::Free(T* a_Ptr)
	{
#ifdef _DEBUG
		BB_ASSERT((a_Ptr >= m_Start && a_Ptr < Pointer::Add(m_Start,  m_Capacity * sizeof(T))), "Trying to free an pool object that is not part of this pool!");
		--m_Size;
#endif // _DEBUG

		//Set the previous free list to the new head.
		(*reinterpret_cast<T**>(a_Ptr)) = reinterpret_cast<T*>(m_Pool);
		//Set the new head.
		m_Pool = reinterpret_cast<T**>(a_Ptr);
	}
}