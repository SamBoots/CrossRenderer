#pragma once
#include "Utils/Logger.h"
#include "BBMemory.h"

#include "Utils/Utils.h"

namespace BB
{
	namespace Array_Specs
	{
		constexpr const size_t multipleValue = 8;
		constexpr const size_t standardSize = 8;
	};

	template<typename T>
	struct Array
	{
		static constexpr bool trivialDestructible_T = std::is_trivially_destructible_v<T>;

		struct Iterator
		{
			//Iterator idea from:
			//https://www.internalpointers.com/post/writing-custom-iterators-modern-cpp

			using iterator_category = std::forward_iterator_tag;
			using difference_type = std::ptrdiff_t;
			using value_type = T;
			using pointer = T*;
			using reference = T&;

			Iterator(pointer a_Ptr) : m_Ptr(a_Ptr) {}

			reference operator*() const { return *m_Ptr; }
			pointer operator->() { return m_Ptr; }

			Iterator& operator++() 
			{ 
				m_Ptr++;  
				return *this;
			}

			Iterator operator++(int) 
			{ 
				Iterator t_Tmp = *this; 
				++(*this);
				return t_Tmp;
			}

			friend bool operator== (const Iterator& a_Lhs, const Iterator& a_Rhs) { return a_Lhs.m_ptr == a_Rhs.m_ptr; };
			friend bool operator!= (const Iterator& a_Lhs, const Iterator& a_Rhs) { return a_Lhs.m_ptr != a_Rhs.m_ptr; };

			friend bool operator< (const Iterator& a_Lhs, const Iterator& a_Rhs) { return a_Lhs.m_ptr < a_Rhs.m_ptr; };
			friend bool operator> (const Iterator& a_Lhs, const Iterator& a_Rhs) { return a_Lhs.m_ptr > a_Rhs.m_ptr; };
			friend bool operator<= (const Iterator& a_Lhs, const Iterator& a_Rhs) { return a_Lhs.m_ptr <= a_Rhs.m_ptr; };
			friend bool operator>= (const Iterator& a_Lhs, const Iterator& a_Rhs) { return a_Lhs.m_ptr >= a_Rhs.m_ptr; };


		private:
			pointer m_Ptr;
		};

		Array(Allocator a_Allocator);
		Array(Allocator a_Allocator, size_t a_Size);
		Array(const Array<T>& a_Array);
		Array(Array<T>&& a_Array) noexcept;
		~Array();

		Array<T>& operator=(const Array<T>& a_Rhs);
		Array<T>& operator=(Array<T>&& a_Rhs) noexcept;
		T& operator[](const size_t a_Index) const;

		void push_back(T& a_Element);
		void push_back(const T* a_Elements, size_t a_Count);
		void insert(size_t a_Position, const T& a_Element);
		void insert(size_t a_Position, const T* a_Elements, size_t a_Count);
		template <class... Args>
		void emplace_back(Args&&... a_Args);
		template <class... Args>
		void emplace(size_t a_Position, Args&&... a_Args);

		void reserve(size_t a_Size);
		void resize(size_t a_Size);

		void pop();
		void clear();

		const size_t size() const { return m_Size; };
		const size_t capacity() const { return m_Capacity; }
		T* data() const { return m_Arr; };

		Iterator begin() { return Iterator(m_Arr); }
		Iterator end() { return Iterator(&m_Arr[m_Size + 1]); } //Get an out of bounds Iterator.
			 
	private:
		void grow(size_t a_MinCapacity = 0);
		//This function also changes the m_Capacity value.
		void reallocate(size_t a_NewCapacity);

		Allocator m_Allocator;

		T* m_Arr;
		size_t m_Size = 0;
		size_t m_Capacity;
	};

	template<typename T>
	inline BB::Array<T>::Array(Allocator a_Allocator)
		: Array(a_Allocator, Array_Specs::standardSize) {}

	template<typename T>
	inline BB::Array<T>::Array(Allocator a_Allocator, size_t a_Size)
		: m_Allocator(a_Allocator)
	{
		BB_EXCEPTION(a_Size != 0, "Dynamic_array size is specified to be 0");
		m_Capacity = Math::RoundUp(a_Size, Array_Specs::multipleValue);

		m_Arr = reinterpret_cast<T*>(BBalloc(m_Allocator, m_Capacity * sizeof(T)));
	}

	template<typename T>
	inline BB::Array<T>::Array(const Array<T>& a_Array)
	{
		m_Allocator = a_Array.m_Allocator;
		m_Size = a_Array.m_Size;
		m_Capacity = a_Array.m_Capacity;
		m_Arr = reinterpret_cast<T*>(BBalloc(m_Allocator, m_Capacity * sizeof(T)));

		Memory::Copy<T>(m_Arr, a_Array.m_Arr, m_Size);
	}

	template<typename T>
	inline BB::Array<T>::Array(Array<T>&& a_Array) noexcept
	{
		m_Allocator = a_Array.m_Allocator;
		m_Size = a_Array.m_Size;
		m_Capacity = a_Array.m_Capacity;
		m_Arr = a_Array.m_Arr;

		a_Array.m_Size = 0;
		a_Array.m_Capacity = 0;
		a_Array.m_Arr = nullptr;
		a_Array.m_Allocator.allocator = nullptr;
		a_Array.m_Allocator.func = nullptr;
	}

	template<typename T>
	inline Array<T>::~Array()
	{
		if (m_Arr != nullptr)
		{
			if constexpr (!trivialDestructible_T)
			{
				for (size_t i = 0; i < m_Size; i++)
				{
					m_Arr[i].~T();
				}
			}

			BBfree(m_Allocator, m_Arr);
		}
	}

	template<typename T>
	inline Array<T>& BB::Array<T>::operator=(const Array<T>& a_Rhs)
	{
		this->~Array();

		m_Allocator = a_Rhs.m_Allocator;
		m_Size = a_Rhs.m_Size;
		m_Capacity = a_Rhs.m_Capacity;
		m_Arr = reinterpret_cast<T*>(BBalloc(m_Allocator, m_Capacity * sizeof(T)));

		Memory::Copy<T>(m_Arr, a_Rhs.m_Arr, m_Size);
		
		return *this;
	}

	template<typename T>
	inline Array<T>& BB::Array<T>::operator=(Array<T>&& a_Rhs) noexcept
	{
		this->~Array();

		m_Allocator = a_Rhs.m_Allocator;
		m_Size = a_Rhs.m_Size;
		m_Capacity = a_Rhs.m_Capacity;
		m_Arr = a_Rhs.m_Arr;

		a_Rhs.m_Size = 0;
		a_Rhs.m_Capacity = 0;
		a_Rhs.m_Arr = nullptr;
		a_Rhs.m_Allocator.allocator = nullptr;
		a_Rhs.m_Allocator.func = nullptr;

		return *this;
	}

	template<typename T>
	inline T& Array<T>::operator[](const size_t a_Index) const
	{
		BB_EXCEPTION(a_Index <= m_Size, "Dynamic_Array, trying to get an element using the [] operator but that element is not there.");
		return m_Arr[a_Index];
	}

	template<typename T>
	inline void Array<T>::push_back(T& a_Element)
	{
		emplace_back(a_Element);
	}

	template<typename T>
	inline void Array<T>::push_back(const T* a_Elements, size_t a_Count)
	{
		if (m_Size + a_Count > m_Capacity)
			grow(a_Count);

		Memory::Copy<T>(m_Arr, a_Elements, a_Count);

		m_Size += a_Count;
	}

	template<typename T>
	inline void BB::Array<T>::insert(size_t a_Position, const T& a_Element)
	{
		emplace(a_Position, a_Element);
	}

	template<typename T>
	template<class ...Args>
	inline void BB::Array<T>::emplace_back(Args&&... a_Args)
	{
		if (m_Size >= m_Capacity)
			grow();

		new (&m_Arr[m_Size]) T(std::forward<Args>(a_Args)...);
		m_Size++;
	}

	template<typename T>
	template<class ...Args>
	inline void BB::Array<T>::emplace(size_t a_Position, Args&&... a_Args)
	{
		BB_ASSERT(m_Size >= a_Position, "trying to insert in a position that is bigger then the current Dynamic_Array size!");
		if (m_Size >= m_Capacity)
			grow();

		if constexpr (!trivialDestructible_T)
		{
			//Move all elements after a_Position 1 to the front.
			for (size_t i = m_Size; i > a_Position; i--)
			{
				new (&m_Arr[i]) T(m_Arr[i - 1]);
				m_Arr[i - 1].~T();
			}
		}
		else
		{
			//Move all elements after a_Position 1 to the front.
			//Using memmove for more safety.
			memmove(&m_Arr[a_Position + 1], &m_Arr[a_Position], sizeof(T) * (m_Size - a_Position));
		}

		new (&m_Arr[a_Position]) T(std::forward<Args>(a_Args)...);
		m_Size++;
	}


	template<typename T>
	inline void Array<T>::reserve(size_t a_Size)
	{
		if (a_Size > m_Capacity)
		{
			size_t t_ModifiedCapacity = Math::RoundUp(a_Size, Array_Specs::multipleValue);

			reallocate(t_ModifiedCapacity);
			return;
		}
	}

	template<typename T>
	inline void BB::Array<T>::resize(size_t a_Size)
	{
		reserve(a_Size);

		for (size_t i = m_Size; i < a_Size; i++)
		{
			new (&m_Arr[i]) T();
		}

		m_Size = a_Size;
	}

	template<typename T>
	inline void BB::Array<T>::pop()
	{
		BB_ASSERT(m_Size != 0, "Dynamic_Array, Popping while m_Size is 0!");
		--m_Size;
		if constexpr (!trivialDestructible_T)
		{
			m_Arr[m_Size].~T();
		}
	}

	template<typename T>
	inline void BB::Array<T>::clear()
	{
		if constexpr (!trivialDestructible_T)
		{
			for (size_t i = 0; i < m_Size; i++)
			{
				m_Arr[i].~T();
			}
		}
		m_Size = 0;
	}

	template<typename T>
	inline void Array<T>::grow(size_t a_MinCapacity)
	{
		size_t t_ModifiedCapacity = m_Capacity * 2;

		if (a_MinCapacity > t_ModifiedCapacity)
			t_ModifiedCapacity = Math::RoundUp(a_MinCapacity, Array_Specs::multipleValue);

		reallocate(t_ModifiedCapacity);
	}

	template<typename T>
	inline void Array<T>::reallocate(size_t a_NewCapacity)
	{
		T* t_NewArr = reinterpret_cast<T*>(BBalloc(m_Allocator, a_NewCapacity * sizeof(T)));

		Memory::Move(t_NewArr, m_Arr, m_Size);
		BBfree(m_Allocator, m_Arr);

		m_Arr = t_NewArr;
		m_Capacity = a_NewCapacity;
	}
}