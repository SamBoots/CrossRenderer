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

	struct String_KeyComp
	{
		bool operator()(const char* a_A, const char* a_B) const
		{
			return strcmp(a_A, a_B) == 0;
		}
	};

	template<typename Key>
	struct Standard_KeyComp
	{
		inline bool operator()(const Key a_A, const Key a_B) const
		{
			return a_A == a_B;
		}
	};

#pragma region Unordered_Map
	//Unordered Map, uses linked list for collision.
	template<typename Key, typename Value, typename KeyComp = Standard_KeyComp<Key>>
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

	public:
		UM_HashMap(Allocator a_Allocator)
			: UM_HashMap(a_Allocator, Hashmap_Specs::Standard_Hashmap_Size)
		{}
		UM_HashMap(Allocator a_Allocator, const size_t a_Size)
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
		UM_HashMap(const UM_HashMap<Key, Value>& a_Map)
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
						t_Entry = reinterpret_cast<HashEntry*>(BBnew(m_Allocator, HashEntry)(*t_Entry->next_Entry));
						t_PreviousEntry->next_Entry = t_Entry;
					}
				}
				else
				{
					new (&m_Entries[i]) HashEntry();
				}
			}
		}
		UM_HashMap(UM_HashMap<Key, Value>&& a_Map) noexcept
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
		~UM_HashMap()
		{
			if (m_Entries != nullptr)
			{
				clear();

				BBfree(m_Allocator, m_Entries);
			}
		}

		UM_HashMap<Key, Value>& operator=(const UM_HashMap<Key, Value>& a_Rhs)
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
						t_Entry = reinterpret_cast<HashEntry*>(BBnew(m_Allocator, HashEntry)(*t_Entry->next_Entry));
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
		UM_HashMap<Key, Value>& operator=(UM_HashMap<Key, Value>&& a_Rhs) noexcept
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

		void insert(const Key& a_Key, Value& a_Res)
		{
			emplace(a_Key, a_Res);
		}
		template <class... Args>
		void emplace(const Key& a_Key, Args&&... a_ValueArgs)
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
		Value* find(const Key& a_Key) const
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
		void erase(const Key& a_Key)
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
		void clear()
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

		void reserve(const size_t a_Size)
		{
			if (a_Size > m_Capacity)
			{
				size_t t_ModifiedCapacity = Math::RoundUp(a_Size, Hashmap_Specs::multipleValue);

				reallocate(t_ModifiedCapacity);
			}
		}

		size_t size() const { return m_Size; }

	private:
		void grow(size_t a_MinCapacity = 1)
		{
			BB_WARNING(false, "Resizing an OL_HashMap, this might be a bit slow. Possibly reserve more.", WarningType::OPTIMALIZATION);

			size_t t_ModifiedCapacity = m_Capacity * 2;

			if (a_MinCapacity > t_ModifiedCapacity)
				t_ModifiedCapacity = Math::RoundUp(a_MinCapacity, Hashmap_Specs::multipleValue);

			reallocate(t_ModifiedCapacity);
		}

		void reallocate(const size_t a_NewLoadCapacity)
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

		size_t m_Capacity;
		size_t m_LoadCapacity;
		size_t m_Size = 0;

		HashEntry* m_Entries;

		Allocator m_Allocator;

	private:
		bool Match(const HashEntry* a_Entry, const Key& a_Key) const
		{
			return KeyComp()(a_Entry->key, a_Key);
		}
	};


#pragma endregion

#pragma region Open Addressing Linear Probing (OL)
	//Open addressing with Linear probing.
	template<typename Key, typename Value, typename KeyComp = Standard_KeyComp<Key>>
	class OL_HashMap
	{
		static constexpr bool trivalDestructableValue = std::is_trivially_destructible_v<Value>;
		static constexpr bool trivalDestructableKey = std::is_trivially_destructible_v<Key>;

	public:
		OL_HashMap(Allocator a_Allocator)
			: OL_HashMap(a_Allocator, Hashmap_Specs::Standard_Hashmap_Size)
		{}
		OL_HashMap(Allocator a_Allocator, const size_t a_Size)
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
		OL_HashMap(const OL_HashMap<Key, Value>& a_Map)
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
		OL_HashMap(OL_HashMap<Key, Value>&& a_Map) noexcept
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
		~OL_HashMap()
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

		OL_HashMap<Key, Value>& operator=(const OL_HashMap<Key, Value>& a_Rhs)
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
		OL_HashMap<Key, Value>& operator=(OL_HashMap<Key, Value>&& a_Rhs) noexcept
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

		void insert(const Key& a_Key, Value& a_Res)
		{
			emplace(a_Key, a_Res);
		}
		template <class... Args>
		void emplace(const Key& a_Key, Args&&... a_ValueArgs)
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
		Value* find(const Key& a_Key) const
		{
			const Hash t_Hash = Hash::MakeHash(a_Key) % m_Capacity;

			for (size_t i = t_Hash; i < m_Capacity; i++)
			{
				if (m_Hashes[i] != Hashmap_Specs::OL_TOMBSTONE && KeyComp()(m_Keys[i], a_Key))
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
				if (m_Hashes[i] != Hashmap_Specs::OL_TOMBSTONE && KeyComp()(m_Keys[i], a_Key))
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
		void erase(const Key& a_Key)
		{
			const Hash t_Hash = Hash::MakeHash(a_Key) % m_Capacity;

			for (size_t i = t_Hash; i < m_Capacity; i++)
			{
				if (KeyComp()(m_Keys[i], a_Key))
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
				if (KeyComp()(m_Keys[i], a_Key))
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
			BB_ASSERT(false, "OL_Hashmap remove called but key not found!");
		}
		void clear()
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

		void reserve(const size_t a_Size)
		{
			if (a_Size > m_Capacity)
			{
				size_t t_ModifiedCapacity = Math::RoundUp(a_Size, Hashmap_Specs::multipleValue);

				reallocate(t_ModifiedCapacity);
			}
		}

		size_t size() const { return m_Size; }
	private:
		void grow(size_t a_MinCapacity = 1)
		{
			BB_WARNING(false, "Resizing an OL_HashMap, this might be a bit slow. Possibly reserve more.", WarningType::OPTIMALIZATION);

			size_t t_ModifiedCapacity = m_Capacity * 2;

			if (a_MinCapacity > t_ModifiedCapacity)
				t_ModifiedCapacity = Math::RoundUp(a_MinCapacity, Hashmap_Specs::multipleValue);

			reallocate(t_ModifiedCapacity);
		}
		void reallocate(const size_t a_NewLoadCapacity)
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
}

#pragma endregion