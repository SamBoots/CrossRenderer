#pragma once
#include "Utils/Logger.h"
#include "BBMemory.h"

namespace BB
{
	//This class is outdated and needs updating.
	template<typename T>
	class Pool
	{
	public:
		Pool(Allocator a_Allocator, const size_t a_Size);
		~Pool();

		T* Get();
		void Free(T* a_Ptr);

		const size_t size() const { return m_Size; };
		const void* data() const { return m_Start; };

	private:
		Allocator m_Allocator;

		size_t m_Capacity;
		size_t m_Size;
		void* m_Start;
		T** m_Pool;
	};

	template<typename T>
	inline BB::Pool<T>::Pool(Allocator a_Allocator, const size_t a_Size)
		: m_Allocator(a_Allocator), m_Capacity(a_Size), m_Size(0)
	{
		BB_STATIC_ASSERT(sizeof(T) >= sizeof(void*), "Pool object is smaller then the size of a pointer");
		BB_STATIC_ASSERT(std::is_trivially_destructible_v<T>, "Pool template Type is not trivially destructable. This is dangerous behaviour");

		m_Start = BBalloc(m_Allocator, m_Capacity * sizeof(T));
		m_Pool = reinterpret_cast<T**>(m_Start);

		T** t_Pool = m_Pool;

		for (size_t i = 0; i < m_Capacity - 1; i++)
		{
			*t_Pool = (reinterpret_cast<T*>(t_Pool)) + 1;
			t_Pool = reinterpret_cast<T**>(*t_Pool);
		}
	}

	template<typename T>
	inline Pool<T>::~Pool()
	{
		//Call destructor of all available...

		BBfree(m_Start);
	}

	template<class T>
	inline T* Pool<T>::Get()
	{

	}

	template<typename T>
	inline void BB::Pool<T>::Free(T* a_Ptr)
	{

	}
}