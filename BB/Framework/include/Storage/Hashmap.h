#pragma once
#include "Utils/Hash.h"
#include "Utils/Utils.h"
#include "BBMemory.h"
namespace BB
{
	namespace Hashmap_Specs
	{
		constexpr uint32_t Standard_Hashmap_Size = 64;

		constexpr const size_t multipleValue = 8;

		constexpr const float UM_LoadFactor = 1.f;
		constexpr const size_t UM_EMPTYNODE = 0xAABBCCDD;


		constexpr const float OL_LoadFactor = 1.3f;
		constexpr const size_t OL_TOMBSTONE = 0xDEADBEEFDEADBEEF;
		constexpr const size_t OL_EMPTY = 0xAABBCCDD;
	};

	//Calculate the load factor.
	static size_t LFCalculation(size_t a_Size, float a_LoadFactor)
	{
		return static_cast<size_t>(static_cast<float>(a_Size) * (1.f / a_LoadFactor + 1.f));
	}

#pragma region Unordered_Map
	//Unordered Map, uses linked list for collision.
	template<typename Key, typename Value>
	class UM_HashMap
	{
		struct HashEntry
		{
			static constexpr bool trivalDestructableKey = std::is_trivially_destructible_v<Key>;
			static constexpr bool trivalDestructableValue = std::is_trivially_destructible_v<Value>;
			~HashEntry()
			{
				state = Hashmap_Specs::UM_EMPTYNODE;
				//Call the destructor if it has one for the value.
				if constexpr (!trivalDestructableValue)
					value.~Value();
				//Call the destructor if it has one for the key.
				if constexpr (!trivalDestructableKey)
					key.~Key();
			}
			HashEntry* next_Entry = nullptr;
			union
			{
				size_t state = Hashmap_Specs::UM_EMPTYNODE;
				Key key;
			};
			Value value;
		};

		struct Iterator
		{
			using value_type = HashEntry;
			using pointer = HashEntry*;
			using reference = HashEntry&;

			Iterator(HashEntry* a_Ptr) : m_MainEntry(a_Ptr), m_Entry(a_Ptr) {};

			reference operator*() { return &m_Entry; }
			pointer operator->() { return m_Entry; }

			Iterator& operator++()
			{
				if (m_Entry->next_Entry == nullptr)
				{
					++m_MainEntry;
					while (m_MainEntry->state == Hashmap_Specs::UM_EMPTYNODE)
					{
						++m_MainEntry;
					}
					m_Entry = m_MainEntry;
				}
				else
				{
					m_Entry = m_Entry->next_Entry;
				}

				return *this;
			}

			Iterator operator++(int)
			{
				Iterator t_Tmp = *this;
				++(*this);
				return t_Tmp;
			}

			friend bool operator< (const Iterator& a_Lhs, const Iterator& a_Rhs) { return a_Lhs.m_MainEntry < a_Rhs.m_MainEntry; };
			friend bool operator> (const Iterator& a_Lhs, const Iterator& a_Rhs) { return a_Lhs.m_MainEntry > a_Rhs.m_MainEntry; };
			friend bool operator<= (const Iterator& a_Lhs, const Iterator& a_Rhs) { return a_Lhs.m_MainEntry <= a_Rhs.m_MainEntry; };
			friend bool operator>= (const Iterator& a_Lhs, const Iterator& a_Rhs) { return a_Lhs.m_MainEntry >= a_Rhs.m_MainEntry; };

		private:
			HashEntry* m_MainEntry;
			HashEntry* m_Entry;
		};

	public:
		UM_HashMap(Allocator a_Allocator);
		UM_HashMap(Allocator a_Allocator, const size_t a_Size);
		UM_HashMap(const UM_HashMap<Key, Value>& a_Map);
		UM_HashMap(UM_HashMap<Key, Value>&& a_Map) noexcept;
		~UM_HashMap();

		UM_HashMap<Key, Value>& operator=(const UM_HashMap<Key, Value>& a_Rhs);
		UM_HashMap<Key, Value>& operator=(UM_HashMap<Key, Value>&& a_Rhs) noexcept;

		void insert(Key& a_Key, Value& a_Res);
		template <class... Args>
		void emplace(Key& a_Key, Args&&... a_ValueArgs);
		Value* find(const Key& a_Key) const;
		void erase(const Key& a_Key);
		void clear();

		void reserve(const size_t a_Size);

		Iterator begin() const;
		Iterator end() const;

		size_t size() const { return m_Size; }

	private:
		void grow(size_t a_MinCapacity = 1);
		void reallocate(const size_t a_NewCapacity);

		size_t m_Capacity;
		size_t m_LoadCapacity;
		size_t m_Size = 0;

		HashEntry* m_Entries;

		Allocator m_Allocator;

	private:
		bool Match(const HashEntry* a_Entry, const Key& a_Key) const
		{
			if (a_Entry->key == a_Key)
			{
				return true;
			}
			return false;
		}
	};

	template<typename Key, typename Value>
	inline UM_HashMap<Key, Value>::UM_HashMap(Allocator a_Allocator)
		: UM_HashMap(a_Allocator, Hashmap_Specs::Standard_Hashmap_Size)
	{}

	template<typename Key, typename Value>
	inline UM_HashMap<Key, Value>::UM_HashMap(Allocator a_Allocator, const size_t a_Size)
		: m_Allocator(a_Allocator)
	{
		m_Capacity = LFCalculation(a_Size, Hashmap_Specs::UM_LoadFactor);
		m_Size = 0;
		m_LoadCapacity = a_Size;
		m_Entries = reinterpret_cast<HashEntry*>(BBalloc(m_Allocator, m_Capacity * sizeof(HashEntry)));

		for (size_t i = 0; i < m_Capacity; i++)
		{
			new (&m_Entries[i]) HashEntry();
		}
	}

	template<typename Key, typename Value>
	inline UM_HashMap<Key, Value>::UM_HashMap(const UM_HashMap<Key, Value>& a_Map)
	{
		m_Allocator = a_Map.m_Allocator;
		m_Size = a_Map.m_Size;
		m_Capacity = a_Map.m_Capacity;
		m_LoadCapacity = a_Map.m_LoadCapacity;

		m_Entries = reinterpret_cast<HashEntry*>(BBalloc(m_Allocator, m_Capacity * sizeof(HashEntry)));

		//Copy over the hashmap and construct the element
		for (size_t i = 0; i < m_Capacity; i++)
		{
			if (a_Map.m_Entries[i].state != Hashmap_Specs::UM_EMPTYNODE)
			{
				new (&m_Entries[i]) HashEntry(a_Map.m_Entries[i]);
				HashEntry* t_Entry = &m_Entries[i];
				HashEntry* t_PreviousEntry;
				while (t_Entry->next_Entry != nullptr)
				{
					t_PreviousEntry = t_Entry;
					t_Entry = reinterpret_cast<HashEntry*>(BBnew(m_Allocator, HashEntry)(* t_Entry->next_Entry));
					t_PreviousEntry->next_Entry = t_Entry;
				}
			}
			else
			{
				new (&m_Entries[i]) HashEntry();
			}
		}
	}

	template<typename Key, typename Value>
	inline UM_HashMap<Key, Value>::UM_HashMap(UM_HashMap<Key, Value>&& a_Map) noexcept
	{
		m_Allocator = a_Map.m_Allocator;
		m_Size = a_Map.m_Size;
		m_Capacity = a_Map.m_Capacity;
		m_LoadCapacity = a_Map.m_LoadCapacity;
		m_Entries = a_Map.m_Entries;

		a_Map.m_Size = 0;
		a_Map.m_Capacity = 0;
		a_Map.m_LoadCapacity = 0;
		a_Map.m_Entries = nullptr;
		a_Map.m_Allocator.allocator = nullptr;
		a_Map.m_Allocator.func = nullptr;
	}

	template<typename Key, typename Value>
	inline UM_HashMap<Key, Value>::~UM_HashMap()
	{
		if (m_Entries != nullptr)
		{ 
			clear();

			BBfree(m_Allocator, m_Entries);
		}
	}

	template<typename Key, typename Value>
	inline UM_HashMap<Key, Value>& BB::UM_HashMap<Key, Value>::operator=(const UM_HashMap<Key, Value>& a_Rhs)
	{
		this->~UM_HashMap();

		m_Allocator = a_Rhs.m_Allocator;
		m_Size = a_Rhs.m_Size;
		m_Capacity = a_Rhs.m_Capacity;
		m_LoadCapacity = a_Rhs.m_LoadCapacity;

		m_Entries = reinterpret_cast<HashEntry*>(BBalloc(m_Allocator, m_Capacity * sizeof(HashEntry)));

		//Copy over the hashmap and construct the element
		for (size_t i = 0; i < m_Capacity; i++)
		{
			if (a_Rhs.m_Entries[i].state != Hashmap_Specs::UM_EMPTYNODE)
			{
				new (&m_Entries[i]) HashEntry(a_Rhs.m_Entries[i]);
				HashEntry* t_Entry = &m_Entries[i];
				HashEntry* t_PreviousEntry;
				while (t_Entry->next_Entry != nullptr)
				{
					t_PreviousEntry = t_Entry;
					t_Entry = reinterpret_cast<HashEntry*>(BBnew(m_Allocator, HashEntry)(* t_Entry->next_Entry));
					t_PreviousEntry->next_Entry = t_Entry;
				}
			}
			else
			{
				new (&m_Entries[i]) HashEntry();
			}
		}

		return *this;
	}

	template<typename Key, typename Value>
	inline UM_HashMap<Key, Value>& BB::UM_HashMap<Key, Value>::operator=(UM_HashMap<Key, Value>&& a_Rhs) noexcept
	{
		this->~UM_HashMap();

		m_Allocator = a_Rhs.m_Allocator;
		m_Size = a_Rhs.m_Size;
		m_Capacity = a_Rhs.m_Capacity;
		m_LoadCapacity = a_Rhs.m_LoadCapacity;
		m_Entries = a_Rhs.m_Entries;

		a_Rhs.m_Size = 0;
		a_Rhs.m_Capacity = 0;
		a_Rhs.m_LoadCapacity = 0;
		a_Rhs.m_Entries = nullptr;
		a_Rhs.m_Allocator.allocator = nullptr;
		a_Rhs.m_Allocator.func = nullptr;

		return *this;
	}

	template<typename Key, typename Value>
	inline void UM_HashMap<Key, Value>::insert(Key& a_Key, Value& a_Res)
	{
		emplace(a_Key, a_Res);
	}

	template<typename Key, typename Value>
	template <class... Args>
	inline void UM_HashMap<Key, Value>::emplace(Key& a_Key, Args&&... a_ValueArgs)
	{
		if (m_Size > m_LoadCapacity)
			grow();

		const Hash t_Hash = Hash::MakeHash(a_Key) % m_Capacity;

		HashEntry* t_Entry = &m_Entries[t_Hash.hash];
		if (t_Entry->state == Hashmap_Specs::UM_EMPTYNODE)
		{
			t_Entry->key = a_Key;
			new (&t_Entry->value) Value(std::forward<Args>(a_ValueArgs)...);
			t_Entry->next_Entry = nullptr;
			return;
		}
		//Collision accurred, no problem we just create a linked list and make a new element.
		//Bad for cache memory though.
		while (t_Entry)
		{
			if (t_Entry->next_Entry == nullptr)
			{
				HashEntry* t_NewEntry = BBnew(m_Allocator, HashEntry);
				t_NewEntry->key = a_Key;
				new (&t_NewEntry->value) Value(std::forward<Args>(a_ValueArgs)...);
				t_NewEntry->next_Entry = nullptr;
				t_Entry->next_Entry = t_NewEntry;
				return;
			}
			t_Entry = t_Entry->next_Entry;
		}
	}

	template<typename Key, typename Value>
	inline Value* UM_HashMap<Key, Value>::find(const Key& a_Key) const
	{
		const Hash t_Hash = Hash::MakeHash(a_Key) % m_Capacity;

		HashEntry* t_Entry = &m_Entries[t_Hash];

		if (t_Entry->state == Hashmap_Specs::UM_EMPTYNODE)
			return nullptr;

		while (t_Entry)
		{
			if (Match(t_Entry, a_Key))
			{
				return &t_Entry->value;
			}
			t_Entry = t_Entry->next_Entry;
		}
		return nullptr;
	}

	template<typename Key, typename Value>
	inline void UM_HashMap<Key, Value>::erase(const Key& a_Key)
	{
		const Hash t_Hash = Hash::MakeHash(a_Key) % m_Capacity;;

		HashEntry* t_Entry = &m_Entries[t_Hash];
		if (Match(t_Entry, a_Key))
		{
			t_Entry->~HashEntry();

			if (t_Entry->next_Entry != nullptr)
			{
				HashEntry* t_NextEntry = t_Entry->next_Entry;
				*t_Entry = *t_Entry->next_Entry;
				BBfree(m_Allocator, t_NextEntry);
				return;
			}

			t_Entry->state = Hashmap_Specs::UM_EMPTYNODE;
			return;
		}

		HashEntry* t_PreviousEntry = nullptr;

		while (t_Entry)
		{
			if (Match(t_Entry, a_Key))
			{
				t_PreviousEntry = t_Entry->next_Entry;
				BBfree(m_Allocator, t_Entry);
				return;
			}
			t_PreviousEntry = t_Entry;
			t_Entry = t_Entry->next_Entry;
		}
	}

	template<typename Key, typename Value>
	inline void UM_HashMap<Key, Value>::clear()
	{
		//go through all the entries and individually delete the extra values from the linked list.
		//They need to be deleted seperatly since the memory is somewhere else.
		for (size_t i = 0; i < m_Capacity; i++)
		{
			if (m_Entries[i].state != Hashmap_Specs::UM_EMPTYNODE)
			{
				HashEntry* t_NextEntry = m_Entries[i].next_Entry;
				while (t_NextEntry != nullptr)
				{
					HashEntry* t_DeleteEntry = t_NextEntry;
					t_NextEntry = t_NextEntry->next_Entry;
					t_DeleteEntry->~HashEntry();

					BBfree(m_Allocator, t_DeleteEntry);
				}
				m_Entries[i].state = Hashmap_Specs::UM_EMPTYNODE;
			}
		}
		for (size_t i = 0; i < m_Capacity; i++)
			if (m_Entries[i].state == Hashmap_Specs::UM_EMPTYNODE)
				m_Entries[i].~HashEntry();

		m_Size = 0;
	}

	template<typename Key, typename Value>
	inline void UM_HashMap<Key, Value>::reserve(const size_t a_Size)
	{
		if (a_Size > m_Capacity)
		{
			size_t t_ModifiedCapacity = Math::RoundUp(a_Size, Hashmap_Specs::multipleValue);

			reallocate(t_ModifiedCapacity);
		}
	}

	template<typename Key, typename Value>
	inline typename UM_HashMap<Key, Value>::Iterator UM_HashMap<Key, Value>::begin() const
	{
		size_t t_FirstFilled = 0;
		while (m_Entries[t_FirstFilled].state == Hashmap_Specs::UM_EMPTYNODE)
		{
			++t_FirstFilled;
		}

		return UM_HashMap<Key, Value>::Iterator(&m_Entries[t_FirstFilled]);
	}

	template<typename Key, typename Value>
	inline typename UM_HashMap<Key, Value>::Iterator UM_HashMap<Key, Value>::end() const
	{
		size_t t_FirstFilled = m_Capacity;
		while (m_Entries[t_FirstFilled].state == Hashmap_Specs::UM_EMPTYNODE)
		{
			--t_FirstFilled;
		}

		return UM_HashMap<Key, Value>::Iterator(&m_Entries[t_FirstFilled]);
	}

	template<typename Key, typename Value>
	inline void UM_HashMap<Key, Value>::grow(size_t a_MinCapacity)
	{
		BB_WARNING(false, "Resizing an OL_HashMap, this might be a bit slow. Possibly reserve more.", WarningType::OPTIMALIZATION);

		size_t t_ModifiedCapacity = m_Capacity * 2;

		if (a_MinCapacity > t_ModifiedCapacity)
			t_ModifiedCapacity = Math::RoundUp(a_MinCapacity, Hashmap_Specs::multipleValue);

		reallocate(t_ModifiedCapacity);
	}

	template<typename Key, typename Value>
	inline void BB::UM_HashMap<Key, Value>::reallocate(const size_t a_NewLoadCapacity)
	{
		const size_t t_NewCapacity = LFCalculation(a_NewLoadCapacity, Hashmap_Specs::UM_LoadFactor);

		//Allocate the new buffer.
		HashEntry* t_NewEntries = reinterpret_cast<HashEntry*>(BBalloc(m_Allocator, t_NewCapacity * sizeof(HashEntry)));

		for (size_t i = 0; i < t_NewCapacity; i++)
		{
			new (&t_NewEntries[i]) HashEntry();
		}

		for (size_t i = 0; i < m_Capacity; i++)
		{
			if (m_Entries[i].state != Hashmap_Specs::UM_EMPTYNODE)
			{
				const Hash t_Hash = Hash::MakeHash(m_Entries[i].key) % t_NewCapacity;

				HashEntry* t_Entry = &t_NewEntries[t_Hash.hash];
				if (t_Entry->state == Hashmap_Specs::UM_EMPTYNODE)
				{
					*t_Entry = m_Entries[i];
				}
				//Collision accurred, no problem we just create a linked list and make a new element.
				//Bad for cache memory though.
				while (t_Entry)
				{
					if (t_Entry->next_Entry == nullptr)
					{
						HashEntry* t_NewEntry = BBnew(m_Allocator, HashEntry)(m_Entries[i]);
					}
					t_Entry = t_Entry->next_Entry;
				}
			}
		}

		this->~UM_HashMap();

		m_Capacity = t_NewCapacity;
		m_LoadCapacity = a_NewLoadCapacity;
		m_Entries = t_NewEntries;
	}

#pragma endregion

#pragma region Open Addressing Linear Probing (OL)
	//Open addressing with Linear probing.
	template<typename Key, typename Value>
	class OL_HashMap
	{
		static constexpr bool trivalDestructableValue = std::is_trivially_destructible_v<Value>;
		static constexpr bool trivalDestructableKey = std::is_trivially_destructible_v<Key>;

	public:
		struct Iterator
		{
			struct Pair
			{
				Key* key{};
				Value* value{};
			};

			using value_type = Pair;
			using pointer = Pair*;
			using reference = Pair&;

			Iterator(Hash* a_HashPtr, Key* a_KeyPtr, Value* a_ValuePtr)
				: m_Hash(a_HashPtr)
			{
				m_Pair.key = a_KeyPtr;
				m_Pair.value = a_ValuePtr;
			};

			reference operator*() { return m_Pair; }
			pointer operator->() { return &m_Pair; }

			Iterator& operator++()
			{
				size_t t_Increase = 1;
				++m_Hash;
				while (*m_Hash == Hashmap_Specs::OL_EMPTY ||
					*m_Hash == Hashmap_Specs::OL_TOMBSTONE)
				{
					++m_Hash;
					++t_Increase;
				}
				m_Pair.key += t_Increase;
				m_Pair.value += t_Increase;
				return *this;
			}

			Iterator operator++(int)
			{
				Iterator t_Tmp = *this;
				++(*this);
				return t_Tmp;
			}

			friend bool operator< (const Iterator& a_Lhs, const Iterator& a_Rhs) { return a_Lhs.m_Hash < a_Rhs.m_Hash; };
			friend bool operator> (const Iterator& a_Lhs, const Iterator& a_Rhs) { return a_Lhs.m_Hash > a_Rhs.m_Hash; };
			friend bool operator<= (const Iterator& a_Lhs, const Iterator& a_Rhs) { return a_Lhs.m_Hash <= a_Rhs.m_Hash; };
			friend bool operator>= (const Iterator& a_Lhs, const Iterator& a_Rhs) { return a_Lhs.m_Hash >= a_Rhs.m_Hash; };

		private:
			Hash* m_Hash;
			value_type m_Pair;
		};

		OL_HashMap(Allocator a_Allocator);
		OL_HashMap(Allocator a_Allocator, const size_t a_Size);
		OL_HashMap(const OL_HashMap<Key, Value>& a_Map);
		OL_HashMap(OL_HashMap<Key, Value>&& a_Map) noexcept;
		~OL_HashMap();

		OL_HashMap<Key, Value>& operator=(const OL_HashMap<Key, Value>& a_Rhs);
		OL_HashMap<Key, Value>& operator=(OL_HashMap<Key, Value>&& a_Rhs) noexcept;

		void insert(Key& a_Key, Value& a_Res);
		template <class... Args>
		void emplace(Key& a_Key, Args&&... a_ValueArgs);
		Value* find(const Key& a_Key) const;
		void erase(const Key& a_Key);
		void clear();

		void reserve(const size_t a_Size);

		size_t size() const { return m_Size; }

		Iterator begin() const;
		Iterator end() const;
	private:
		void grow(size_t a_MinCapacity = 1);
		void reallocate(const size_t a_NewLoadCapacity);

	private:
		size_t m_Capacity;
		size_t m_Size;
		size_t m_LoadCapacity;

		//All the elements.
		Hash* m_Hashes;
		Key* m_Keys;
		Value* m_Values;

		Allocator m_Allocator;
	};

	template<typename Key, typename Value>
	inline OL_HashMap<Key, Value>::OL_HashMap(Allocator a_Allocator)
		: OL_HashMap(a_Allocator, Hashmap_Specs::Standard_Hashmap_Size)
	{}

	template<typename Key, typename Value>
	inline OL_HashMap<Key, Value>::OL_HashMap(Allocator a_Allocator, const size_t a_Size)
		: m_Allocator(a_Allocator)
	{
		m_Capacity = LFCalculation(a_Size, Hashmap_Specs::OL_LoadFactor);
		m_Size = 0;
		m_LoadCapacity = a_Size;

		const size_t t_MemorySize = (sizeof(Hash) + sizeof(Key) + sizeof(Value)) * m_Capacity;

		void* t_Buffer = BBalloc(m_Allocator, t_MemorySize);
		m_Hashes = reinterpret_cast<Hash*>(t_Buffer);
		m_Keys = reinterpret_cast<Key*>(Pointer::Add(t_Buffer, sizeof(Hash) * m_Capacity));
		m_Values = reinterpret_cast<Value*>(Pointer::Add(t_Buffer, (sizeof(Hash) + sizeof(Key)) * m_Capacity));
		for (size_t i = 0; i < m_Capacity; i++)
		{
			m_Hashes[i] = Hashmap_Specs::OL_EMPTY;
		}
	}

	template<typename Key, typename Value>
	inline OL_HashMap<Key, Value>::OL_HashMap(const OL_HashMap<Key, Value>& a_Map)
	{
		m_Capacity = a_Map.m_Capacity;
		m_Size = 0;
		m_LoadCapacity = a_Map.m_LoadCapacity;

		m_Allocator = a_Map.m_Allocator;

		const size_t t_MemorySize = (sizeof(Hash) + sizeof(Key) + sizeof(Value)) * m_Capacity;

		void* t_Buffer = BBalloc(m_Allocator, t_MemorySize);
		m_Hashes = reinterpret_cast<Hash*>(t_Buffer);
		m_Keys = reinterpret_cast<Key*>(Pointer::Add(t_Buffer, sizeof(Hash) * m_Capacity));
		m_Values = reinterpret_cast<Value*>(Pointer::Add(t_Buffer, (sizeof(Hash) + sizeof(Key)) * m_Capacity));
		for (size_t i = 0; i < m_Capacity; i++)
		{
			m_Hashes[i] = Hashmap_Specs::OL_EMPTY;
		}

		for (size_t i = 0; i < m_Capacity; i++)
		{
			if (a_Map.m_Hashes[i] != Hashmap_Specs::OL_EMPTY && a_Map.m_Hashes[i] != Hashmap_Specs::OL_TOMBSTONE)
			{
				insert(a_Map.m_Keys[i], a_Map.m_Values[i]);
			}
		}
	}

	template<typename Key, typename Value>
	inline OL_HashMap<Key, Value>::OL_HashMap(OL_HashMap<Key, Value>&& a_Map) noexcept
	{
		m_Capacity = a_Map.m_Capacity;
		m_Size = a_Map.m_Size;
		m_LoadCapacity = a_Map.m_LoadCapacity;

		m_Hashes = a_Map.m_Hashes;
		m_Keys = a_Map.m_Keys;
		m_Values = a_Map.m_Values;

		m_Allocator = a_Map.m_Allocator;

		a_Map.m_Capacity = 0;
		a_Map.m_Size = 0;
		a_Map.m_LoadCapacity = 0;
		a_Map.m_Hashes = nullptr;
		a_Map.m_Keys = nullptr;
		a_Map.m_Values = nullptr;

		a_Map.m_Allocator.allocator = nullptr;
		a_Map.m_Allocator.func = nullptr;
	}

	template<typename Key, typename Value>
	inline OL_HashMap<Key, Value>::~OL_HashMap()
	{
		if (m_Hashes != nullptr)
		{
			//Call the destructor if it has one for the value.
			if constexpr (!trivalDestructableValue)
				for (size_t i = 0; i < m_Capacity; i++)
					if (m_Hashes[i] != 0)
						m_Values[i].~Value();
			//Call the destructor if it has one for the key.
			if constexpr (!trivalDestructableKey)
				for (size_t i = 0; i < m_Capacity; i++)
					if (m_Hashes[i] != 0)
						m_Keys[i].~Key();

			BBfree(m_Allocator, m_Hashes);
		}
	}

	template<typename Key, typename Value>
	inline OL_HashMap<Key, Value>& BB::OL_HashMap<Key, Value>::operator=(const OL_HashMap<Key, Value>& a_Rhs)
	{
		this->~OL_HashMap();

		m_Capacity = a_Rhs.m_Capacity;
		m_Size = 0;
		m_LoadCapacity = a_Rhs.m_LoadCapacity;

		m_Allocator = a_Rhs.m_Allocator;

		const size_t t_MemorySize = (sizeof(Hash) + sizeof(Key) + sizeof(Value)) * m_Capacity;

		void* t_Buffer = BBalloc(m_Allocator, t_MemorySize);
		m_Hashes = reinterpret_cast<Hash*>(t_Buffer);
		m_Keys = reinterpret_cast<Key*>(Pointer::Add(t_Buffer, sizeof(Hash) * m_Capacity));
		m_Values = reinterpret_cast<Value*>(Pointer::Add(t_Buffer, (sizeof(Hash) + sizeof(Key)) * m_Capacity));
		for (size_t i = 0; i < m_Capacity; i++)
		{
			m_Hashes[i] = Hashmap_Specs::OL_EMPTY;
		}

		for (size_t i = 0; i < m_Capacity; i++)
		{
			if (a_Rhs.m_Hashes[i] != Hashmap_Specs::OL_EMPTY && a_Rhs.m_Hashes[i] != Hashmap_Specs::OL_TOMBSTONE)
			{
				insert(a_Rhs.m_Keys[i], a_Rhs.m_Values[i]);
			}
		}

		return *this;
	}

	template<typename Key, typename Value>
	inline OL_HashMap<Key, Value>& BB::OL_HashMap<Key, Value>::operator=(OL_HashMap<Key, Value>&& a_Rhs) noexcept
	{
		this->~OL_HashMap();

		m_Capacity = a_Rhs.m_Capacity;
		m_Size = a_Rhs.m_Size;
		m_LoadCapacity = a_Rhs.m_LoadCapacity;

		m_Allocator = a_Rhs.m_Allocator;

		m_Hashes = a_Rhs.m_Hashes;
		m_Keys = a_Rhs.m_Keys;
		m_Values = a_Rhs.m_Values;

		a_Rhs.m_Capacity = 0;
		a_Rhs.m_Size = 0;
		a_Rhs.m_LoadCapacity = 0;

		a_Rhs.m_Allocator.allocator = nullptr;
		a_Rhs.m_Allocator.func = nullptr;

		a_Rhs.m_Hashes = nullptr;
		a_Rhs.m_Keys = nullptr;
		a_Rhs.m_Values = nullptr;

		return *this;
	}

	template<typename Key, typename Value>
	inline void OL_HashMap<Key, Value>::insert(Key& a_Key, Value& a_Res)
	{
		emplace(a_Key, a_Res);
	}

	template<typename Key, typename Value>
	template <class... Args>
	inline void OL_HashMap<Key, Value>::emplace(Key& a_Key, Args&&... a_ValueArgs)
	{
		if (m_Size > m_LoadCapacity)
			grow();

		m_Size++;
		const Hash t_Hash = Hash::MakeHash(a_Key) % m_Capacity;


		for (size_t i = t_Hash; i < m_Capacity; i++)
		{
			if (m_Hashes[i] == Hashmap_Specs::OL_EMPTY || m_Hashes[i] == Hashmap_Specs::OL_TOMBSTONE)
			{
				m_Hashes[i] = t_Hash;
				m_Keys[i] = a_Key;
				new (&m_Values[i]) Value(std::forward<Args>(a_ValueArgs)...);
				return;
			}
		}

		//Loop again but then from the start and stop at the hash. 
		for (size_t i = 0; i < t_Hash; i++)
		{
			if (m_Hashes[i] == Hashmap_Specs::OL_EMPTY || m_Hashes[i] == Hashmap_Specs::OL_TOMBSTONE)
			{
				m_Hashes[i] = t_Hash;
				m_Keys[i] = a_Key;
				new (&m_Values[i]) Value(std::forward<Args>(a_ValueArgs)...);
				return;
			}
		}
	}

	template<typename Key, typename Value>
	inline Value* OL_HashMap<Key, Value>::find(const Key& a_Key) const
	{
		const Hash t_Hash = Hash::MakeHash(a_Key) % m_Capacity;

		for (size_t i = t_Hash; i < m_Capacity; i++)
		{
			if (m_Hashes[i] != Hashmap_Specs::OL_TOMBSTONE && m_Keys[i] == a_Key)
			{
				return &m_Values[i];
			}
			//If you hit an empty return a nullptr.
			if (m_Hashes[i] == Hashmap_Specs::OL_EMPTY)
			{
				return nullptr;
			}
		}

		//Loop again but then from the start and stop at the hash. 
		for (size_t i = 0; i < t_Hash; i++)
		{
			if (m_Hashes[i] != Hashmap_Specs::OL_TOMBSTONE && m_Keys[i] == a_Key)
			{
				return &m_Values[i];
			}
			//If you hit an empty return a nullptr.
			if (m_Hashes[i] == Hashmap_Specs::OL_EMPTY)
			{
				return nullptr;
			}
		}

		//Key does not exist.
		return nullptr;
	}

	template<typename Key, typename Value>
	inline void OL_HashMap<Key, Value>::erase(const Key& a_Key)
	{
		const Hash t_Hash = Hash::MakeHash(a_Key) % m_Capacity;

		for (size_t i = t_Hash; i < m_Capacity; i++)
		{
			if (m_Keys[i] == a_Key)
			{
				m_Hashes[i] = Hashmap_Specs::OL_TOMBSTONE;
				//Call the destructor if it has one for the value.
				if constexpr (!trivalDestructableValue)
					m_Values[i].~Value();
				//Call the destructor if it has one for the key.
				if constexpr (!trivalDestructableKey)
					m_Keys[i].~Key();
				m_Keys[i] = 0;

				m_Size--;
				return;
			}
		}

		//Loop again but then from the start and stop at the hash. 
		for (size_t i = 0; i < t_Hash; i++)
		{
			if (m_Keys[i] == a_Key)
			{
				m_Hashes[i] = Hashmap_Specs::OL_TOMBSTONE;
				//Call the destructor if it has one for the value.
				if constexpr (!trivalDestructableValue)
					m_Values[i].~Value();
				//Call the destructor if it has one for the key.
				if constexpr (!trivalDestructableKey)
					m_Keys[i].~Key();
				m_Keys[i] = 0;

				m_Size--;
				return;
			}
		}
		BB_EXCEPTION(false, "OL_Hashmap remove called but key not found!");
	}

	template<typename Key, typename Value>
	inline void OL_HashMap<Key, Value>::clear()
	{
		for (size_t i = 0; i < m_Capacity; i++)
		{
			if (m_Hashes[i] != Hashmap_Specs::OL_EMPTY)
			{
				m_Hashes[i] = Hashmap_Specs::OL_EMPTY;
				if constexpr (!trivalDestructableValue)
					m_Values[i].~Value();
				if constexpr (!trivalDestructableKey)
					m_Keys[i].~Key();
				m_Keys[i] = 0;
			}
		}
		m_Size = 0;
	}

	template<typename Key, typename Value>
	inline void OL_HashMap<Key, Value>::reserve(const size_t a_Size)
	{
		if (a_Size > m_Capacity)
		{
			size_t t_ModifiedCapacity = Math::RoundUp(a_Size, Hashmap_Specs::multipleValue);

			reallocate(t_ModifiedCapacity);
		}
	}

	template<typename Key, typename Value>
	inline typename OL_HashMap<Key, Value>::Iterator OL_HashMap<Key, Value>::begin() const
	{
		size_t t_FirstFilled = 0;
		while (m_Hashes[t_FirstFilled] == Hashmap_Specs::OL_EMPTY ||
			m_Hashes[t_FirstFilled] == Hashmap_Specs::OL_TOMBSTONE)
		{
			++t_FirstFilled;
		}

		return OL_HashMap<Key, Value>::Iterator(&m_Hashes[t_FirstFilled], &m_Keys[t_FirstFilled], &m_Values[t_FirstFilled]);
	}

	template<typename Key, typename Value>
	inline typename OL_HashMap<Key, Value>::Iterator OL_HashMap<Key, Value>::end() const
	{
		size_t t_FirstFilled = m_Capacity; 
		while (m_Hashes[t_FirstFilled] == Hashmap_Specs::OL_EMPTY ||
			m_Hashes[t_FirstFilled] == Hashmap_Specs::OL_TOMBSTONE)
		{
			--t_FirstFilled;
		}

		return OL_HashMap<Key, Value>::Iterator(&m_Hashes[t_FirstFilled], &m_Keys[t_FirstFilled], &m_Values[t_FirstFilled]);
	}

	template<typename Key, typename Value>
	inline void OL_HashMap<Key, Value>::grow(size_t a_MinCapacity)
	{
		BB_WARNING(false, "Resizing an OL_HashMap, this might be a bit slow. Possibly reserve more.", WarningType::OPTIMALIZATION);

		size_t t_ModifiedCapacity = m_Capacity * 2;

		if (a_MinCapacity > t_ModifiedCapacity)
			t_ModifiedCapacity = Math::RoundUp(a_MinCapacity, Hashmap_Specs::multipleValue);

		reallocate(t_ModifiedCapacity);
	}

	template<typename Key, typename Value>
	inline void OL_HashMap<Key, Value>::reallocate(const size_t a_NewLoadCapacity)
	{
		const size_t t_NewCapacity = LFCalculation(a_NewLoadCapacity, Hashmap_Specs::OL_LoadFactor);

		//Allocate the new buffer.
		const size_t t_MemorySize = (sizeof(Hash) + sizeof(Key) + sizeof(Value)) * t_NewCapacity;
		void* t_Buffer = BBalloc(m_Allocator, t_MemorySize);

		Hash* t_NewHashes = reinterpret_cast<Hash*>(t_Buffer);
		Key* t_NewKeys = reinterpret_cast<Key*>(Pointer::Add(t_Buffer, sizeof(Hash) * t_NewCapacity));
		Value* t_NewValues = reinterpret_cast<Value*>(Pointer::Add(t_Buffer, (sizeof(Hash) + sizeof(Key)) * t_NewCapacity));
		for (size_t i = 0; i < t_NewCapacity; i++)
		{
			t_NewHashes[i] = Hashmap_Specs::OL_EMPTY;
		}

		for (size_t i = 0; i < m_Capacity; i++)
		{
			if (m_Hashes[i] == i)
			{
				Key t_Key = m_Keys[i];
				Hash t_Hash = Hash::MakeHash(t_Key) % t_NewCapacity;

				while (t_NewHashes[t_Hash] != Hashmap_Specs::OL_EMPTY)
				{
					t_Hash++;
					if (t_Hash > t_NewCapacity)
						t_Hash = 0;
				}
				t_NewHashes[t_Hash] = t_Hash;
				t_NewKeys[t_Hash] = t_Key;
				t_NewValues[t_Hash] = m_Values[i];
			}
		}

		//Remove all the elements and free the memory.
		this->~OL_HashMap();

		m_Hashes = t_NewHashes;
		m_Keys = t_NewKeys;
		m_Values = t_NewValues;

		m_Capacity = t_NewCapacity;
		m_LoadCapacity = a_NewLoadCapacity;
	}
}

#pragma endregion