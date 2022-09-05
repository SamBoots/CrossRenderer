#pragma once
#include "Allocators/AllocTypes.h"
#include "Utils/Utils.h"

namespace BB
{
	namespace Slotmap_Specs
	{
		constexpr const size_t multipleValue = 8;
		constexpr const size_t standardSize = 8;
	}

	using SlotmapID = size_t;

	template <typename T>
	class Slotmap
	{
		static constexpr bool trivialDestructible_T = std::is_trivially_destructible_v<T>;

		struct Node
		{
			SlotmapID id;
			T value;
		};

	public:
		struct Iterator
		{
			Iterator(Node* a_Ptr) : m_Ptr(a_Ptr) {}

			Node& operator*() const { return *m_Ptr; }
			Node* operator->() { return m_Ptr; }

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

			friend bool operator< (const Iterator& a_Lhs, const Iterator& a_Rhs) { return a_Lhs.m_Ptr < a_Rhs.m_Ptr; };
			friend bool operator> (const Iterator& a_Lhs, const Iterator& a_Rhs) { return a_Lhs.m_Ptr > a_Rhs.m_Ptr; };
			friend bool operator<= (const Iterator& a_Lhs, const Iterator& a_Rhs) { return a_Lhs.m_Ptr <= a_Rhs.m_Ptr; };
			friend bool operator>= (const Iterator& a_Lhs, const Iterator& a_Rhs) { return a_Lhs.m_Ptr >= a_Rhs.m_Ptr; };

		private:
			Node* m_Ptr;
		};

		Slotmap(Allocator a_Allocator);
		Slotmap(Allocator a_Allocator, size_t a_Size);
		Slotmap(const Slotmap<T>& a_Map);
		Slotmap(Slotmap<T>&& a_Map) noexcept;
		~Slotmap();

		Slotmap<T>& operator=(const Slotmap<T>& a_Rhs);
		Slotmap<T>& operator=(Slotmap<T>&& a_Rhs) noexcept;

		SlotmapID insert(T& a_Obj);
		template <class... Args>
		SlotmapID emplace(Args&&... a_Args);
		T& find(SlotmapID a_ID);
		void erase(SlotmapID a_ID);

		void reserve(size_t a_Capacity);

		void clear();

		Iterator begin() { return Iterator(m_ObjArr); }
		Iterator end() { return Iterator(&m_ObjArr[m_Size]); }

		size_t size() const { return m_Size; }

	private:
		void grow();
		//This function also changes the m_Capacity value.
		void reallocate(size_t a_NewCapacity);

		Allocator m_Allocator;

		SlotmapID* m_IdArr;
		Node* m_ObjArr;

		size_t m_Capacity = 128;
		size_t m_Size = 0;
		SlotmapID m_NextFree;
	};

	template<typename T>
	inline BB::Slotmap<T>::Slotmap(Allocator a_Allocator)
		:	Slotmap(a_Allocator, Slotmap_Specs::standardSize)
	{}

	template<typename T>
	inline BB::Slotmap<T>::Slotmap(Allocator a_Allocator, size_t a_Size)
	{
		m_Allocator = a_Allocator;
		m_Capacity = a_Size;

		m_IdArr = reinterpret_cast<SlotmapID*>(BBalloc(m_Allocator, sizeof(SlotmapID) * m_Capacity));
		m_ObjArr = reinterpret_cast<Node*>(BBalloc(m_Allocator, sizeof(Node) * m_Capacity));

		for (size_t i = 0; i < m_Capacity; ++i)
		{
			m_IdArr[i] = i + 1;
		}
		m_NextFree = 0;
	}

	template<typename T>
	inline BB::Slotmap<T>::Slotmap(const Slotmap<T>& a_Map)
	{
		m_Allocator = a_Map.m_Allocator;
		m_Capacity = a_Map.m_Capacity;
		m_Size = a_Map.m_Size;
		m_NextFree = a_Map.m_NextFree;

		m_IdArr = reinterpret_cast<SlotmapID*>(BBalloc(m_Allocator, sizeof(SlotmapID) * m_Capacity));
		m_ObjArr = reinterpret_cast<Node*>(BBalloc(m_Allocator, sizeof(Node) * m_Capacity));

		BB::Memory::Copy(m_IdArr, a_Map.m_IdArr, m_Capacity);
		BB::Memory::Copy(m_ObjArr, a_Map.m_ObjArr, m_Size);
	}

	template<typename T>
	inline BB::Slotmap<T>::Slotmap(Slotmap<T>&& a_Map) noexcept
	{
		m_Capacity = a_Map.m_Capacity;
		m_Size = a_Map.m_Size;
		m_NextFree = a_Map.m_NextFree;
		m_IdArr = a_Map.m_IdArr;
		m_ObjArr = a_Map.m_ObjArr;
		m_Allocator = a_Map.m_Allocator;

		a_Map.m_Capacity = 0;
		a_Map.m_Size = 0;
		a_Map.m_NextFree = 0;
		a_Map.m_IdArr = nullptr;
		a_Map.m_ObjArr = nullptr;
		a_Map.m_Allocator.allocator = nullptr;
		a_Map.m_Allocator.func = nullptr;
	}

	template<typename T>
	inline BB::Slotmap<T>::~Slotmap()
	{
		if (m_IdArr != nullptr)
		{
			if constexpr (!trivialDestructible_T)
			{
				for (size_t i = 0; i < m_Size; i++)
				{
					m_ObjArr[i].value.~T();
				}
			}

			BBfree(m_Allocator, m_IdArr);
			BBfree(m_Allocator, m_ObjArr);
		}
	}

	template<typename T>
	inline Slotmap<T>& BB::Slotmap<T>::operator=(const Slotmap<T>& a_Rhs)
	{
		this->~Slotmap();

		m_Allocator = a_Rhs.m_Allocator;
		m_Capacity = a_Rhs.m_Capacity;
		m_Size = a_Rhs.m_Size;
		m_NextFree = a_Rhs.m_NextFree;

		m_IdArr = reinterpret_cast<SlotmapID*>(BBalloc(m_Allocator, sizeof(SlotmapID) * m_Capacity));
		m_ObjArr = reinterpret_cast<Node*>(BBalloc(m_Allocator, sizeof(Node) * m_Capacity));

		BB::Memory::Copy(m_IdArr, a_Rhs.m_IdArr, m_Capacity);
		BB::Memory::Copy(m_ObjArr, a_Rhs.m_ObjArr, m_Size);

		return *this;
	}

	template<typename T>
	inline Slotmap<T>& BB::Slotmap<T>::operator=(Slotmap<T>&& a_Rhs) noexcept
	{
		this->~Slotmap();

		m_Capacity = a_Rhs.m_Capacity;
		m_Size = a_Rhs.m_Size;
		m_NextFree = a_Rhs.m_NextFree;
		m_IdArr = a_Rhs.m_IdArr;
		m_ObjArr = a_Rhs.m_ObjArr;
		m_Allocator = a_Rhs.m_Allocator;

		a_Rhs.m_Capacity = 0;
		a_Rhs.m_Size = 0;
		a_Rhs.m_NextFree = 0;
		a_Rhs.m_IdArr = nullptr;
		a_Rhs.m_ObjArr = nullptr;
		a_Rhs.m_Allocator.allocator = nullptr;
		a_Rhs.m_Allocator.func = nullptr;

		return *this;
	}

	template<typename T>
	inline SlotmapID BB::Slotmap<T>::insert(T& a_Obj)
	{
		return emplace(a_Obj);
	}

	template<typename T>
	template<class ...Args>
	inline SlotmapID BB::Slotmap<T>::emplace(Args&&... a_Args)
	{
		if (m_Size >= m_Capacity)
			grow();

		SlotmapID& t_Id = m_IdArr[m_NextFree];
		m_NextFree = t_Id;

		t_Id = m_Size++;

		Node& t_Node = m_ObjArr[t_Id];
		t_Node.id = t_Id;
		new (&t_Node.value) T(std::forward<Args>(a_Args)...);

		return t_Id;
	}

	template<typename T>
	inline T& BB::Slotmap<T>::find(SlotmapID a_ID)
	{
		return m_ObjArr[a_ID].value;
	}

	template<typename T>
	inline void BB::Slotmap<T>::erase(SlotmapID a_ID)
	{
		size_t t_OldFree = m_NextFree;
		m_NextFree = a_ID;
		m_IdArr[a_ID] = t_OldFree;

		Slotmap::Node& t_Node = m_ObjArr[--m_Size];
		t_Node.id = t_OldFree;
		if constexpr (!trivialDestructible_T)
		{
			//Before move call the destructor if it has one.
			t_Node.value.~T();
		}

		t_Node.value = std::move(m_ObjArr[a_ID].value);
	}
	template<typename T>
	inline void BB::Slotmap<T>::reserve(size_t a_Capacity)
	{
		if (a_Capacity > m_Capacity)
			reallocate(a_Capacity);
	}


	template<typename T>
	inline void BB::Slotmap<T>::clear()
	{
		m_Size = 0;

		for (size_t i = 0; i < m_Capacity; ++i)
		{
			m_IdArr[i] = i + 1;
		}
		m_NextFree = 0;

		//Destruct all the variables when it is not trivially destructable.
		if constexpr (!trivialDestructible_T)
		{
			for (size_t i = 0; i < m_Size; i++)
			{
				m_ObjArr[i].value.~T();
			}
		}
	}

	template<typename T>
	inline void BB::Slotmap<T>::grow()
	{
		reallocate(m_Capacity * 2);
	}

	template<typename T>
	inline void BB::Slotmap<T>::reallocate(size_t a_NewCapacity)
	{
		SlotmapID* t_NewIdArr = reinterpret_cast<SlotmapID*>(BBalloc(m_Allocator, sizeof(SlotmapID) * a_NewCapacity));
		Node* t_NewObjArr = reinterpret_cast<Node*>(BBalloc(m_Allocator, sizeof(Node) * a_NewCapacity));

		BB::Memory::Copy(t_NewIdArr, m_IdArr, m_Capacity);
		BB::Memory::Copy(t_NewObjArr, m_ObjArr, m_Size);

		for (size_t i = m_Capacity; i < a_NewCapacity; ++i)
		{
			t_NewIdArr[i] = i + 1;
		}

		this->~Slotmap();

		m_Capacity = a_NewCapacity;
		m_IdArr = t_NewIdArr;
		m_ObjArr = t_NewObjArr;
	}
}