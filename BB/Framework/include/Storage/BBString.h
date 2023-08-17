#pragma once
#include "Utils/Logger.h"
#include "BBMemory.h"

#include "Utils/Utils.h"

namespace BB
{
	namespace String_Specs
	{
		constexpr const size_t multipleValue = 8;
		constexpr const size_t standardSize = 8;
	}

	template<typename CharT>
	class Basic_String
	{
	public:
		Basic_String(Allocator a_Allocator);
		Basic_String(Allocator a_Allocator, size_t a_Size);
		Basic_String(Allocator a_Allocator, const CharT* a_String);
		Basic_String(Allocator a_Allocator, const CharT* a_String, size_t a_Size);
		Basic_String(const Basic_String<CharT>& a_String);
		Basic_String(Basic_String<CharT>&& a_String) noexcept;
		~Basic_String();

		Basic_String& operator=(const Basic_String<CharT>& a_Rhs);
		Basic_String& operator=(Basic_String<CharT>&& a_Rhs) noexcept;
		bool operator==(const Basic_String<CharT>& a_Rhs) const;

		void append(const Basic_String<CharT>& a_String);
		void append(const Basic_String<CharT>& a_String, size_t a_SubPos, size_t a_SubLength);
		void append(const CharT* a_String);
		void append(const CharT* a_String, size_t a_Size);
		void insert(size_t a_Pos, const Basic_String<CharT>& a_String);
		void insert(size_t a_Pos, const Basic_String<CharT>& a_String, size_t a_SubPos, size_t a_SubLength);
		void insert(size_t a_Pos, const CharT* a_String);
		void insert(size_t a_Pos, const CharT* a_String, size_t a_Size);
		void push_back(const CharT a_Char);
		
		void pop_back();

		bool compare(const Basic_String<CharT>& a_String) const;
		bool compare(const Basic_String<CharT>& a_String, size_t a_Size) const;
		bool compare(size_t a_Pos, const Basic_String<CharT>& a_String, size_t a_Subpos, size_t a_Size) const;
		bool compare(const CharT* a_String) const;
		bool compare(const CharT* a_String, size_t a_Size) const;
		bool compare(size_t a_Pos, const CharT* a_String) const;
		bool compare(size_t a_Pos, const CharT* a_String, size_t a_Size) const;

		void clear();

		void reserve(const size_t a_Size);
		void shrink_to_fit();

		size_t size() const { return m_Size; }
		size_t capacity() const { return m_Capacity; }
		CharT* data() const { return m_String; }
		const CharT* c_str() const { return m_String; }

	private:
		void grow(size_t a_MinCapacity = 1);
		void reallocate(size_t a_NewCapacity);

		Allocator m_Allocator;

		CharT* m_String;
		size_t m_Size = 0;
		size_t m_Capacity = 64;
	};

	using String = Basic_String<char>;
	using WString = Basic_String<wchar_t>;


	template<typename CharT>
	inline BB::Basic_String<CharT>::Basic_String(Allocator a_Allocator)
		: Basic_String(a_Allocator, String_Specs::standardSize)
	{}

	template<typename CharT>
	inline BB::Basic_String<CharT>::Basic_String(Allocator a_Allocator, size_t a_Size)
	{
		constexpr bool is_char = std::is_same_v<CharT, char> || std::is_same_v<CharT, wchar_t>;
		BB_STATIC_ASSERT(is_char, "String is not a char or wchar");

		m_Allocator = a_Allocator;
		m_Capacity = Math::RoundUp(a_Size, String_Specs::multipleValue);

		m_String = reinterpret_cast<CharT*>(BBalloc(m_Allocator, m_Capacity * sizeof(CharT)));
		Memory::Set(m_String, NULL, m_Capacity);
	}

	template<typename CharT>
	inline BB::Basic_String<CharT>::Basic_String(Allocator a_Allocator, const CharT* a_String)
		:	Basic_String(a_Allocator, a_String, Memory::StrLength(a_String))
	{}

	template<typename CharT>
	inline BB::Basic_String<CharT>::Basic_String(Allocator a_Allocator, const CharT* a_String, size_t a_Size)
	{
		constexpr bool is_char = std::is_same_v<CharT, char> || std::is_same_v<CharT, wchar_t>;
		BB_STATIC_ASSERT(is_char, "String is not a char or wchar");

		m_Allocator = a_Allocator;
		m_Capacity = Math::RoundUp(a_Size + 1, String_Specs::multipleValue);
		m_Size = a_Size;

		m_String = reinterpret_cast<CharT*>(BBalloc(m_Allocator, m_Capacity * sizeof(CharT)));
		Memory::Copy(m_String, a_String, a_Size);
		Memory::Set(m_String + a_Size, NULL, m_Capacity - a_Size);
	}

	template<typename CharT>
	inline BB::Basic_String<CharT>::Basic_String(const Basic_String<CharT>& a_String)
	{
		m_Allocator = a_String.m_Allocator;
		m_Capacity = a_String.m_Capacity;
		m_Size = a_String.m_Size;

		m_String = reinterpret_cast<CharT*>(BBalloc(m_Allocator, m_Capacity * sizeof(CharT)));
		Memory::Copy(m_String, a_String.m_String, m_Capacity);
	}

	template<typename CharT>
	inline BB::Basic_String<CharT>::Basic_String(Basic_String<CharT>&& a_String) noexcept
	{
		m_Allocator = a_String.m_Allocator;
		m_Capacity = a_String.m_Capacity;
		m_Size = a_String.m_Size;
		m_String = a_String.m_String;

		a_String.m_Allocator.allocator = nullptr;
		a_String.m_Allocator.func = nullptr;
		a_String.m_Capacity = 0;
		a_String.m_Size = 0;
		a_String.m_String = nullptr;
	}

	template<typename CharT>
	inline BB::Basic_String<CharT>::~Basic_String()
	{
		if (m_String != nullptr)
		{
			BBfree(m_Allocator, m_String);
			m_String = nullptr;
		}
	}

	template<typename CharT>
	inline Basic_String<CharT>& BB::Basic_String<CharT>::operator=(const Basic_String<CharT>& a_Rhs)
	{
		this->~Basic_String();

		m_Allocator = a_Rhs.m_Allocator;
		m_Capacity = a_Rhs.m_Capacity;
		m_Size = a_Rhs.m_Size;

		m_String = reinterpret_cast<CharT*>(BBalloc(m_Allocator, m_Capacity * sizeof(CharT)));
		Memory::Copy(m_String, a_Rhs.m_String, m_Capacity);

		return *this;
	}

	template<typename CharT>
	inline Basic_String<CharT>& BB::Basic_String<CharT>::operator=(Basic_String<CharT>&& a_Rhs) noexcept
	{
		this->~Basic_String();

		m_Allocator = a_Rhs.m_Allocator;
		m_Capacity = a_Rhs.m_Capacity;
		m_Size = a_Rhs.m_Size;
		m_String = a_Rhs.m_String;

		a_Rhs.m_Allocator.allocator = nullptr;
		a_Rhs.m_Allocator.func = nullptr;
		a_Rhs.m_Capacity = 0;
		a_Rhs.m_Size = 0;
		a_Rhs.m_String = nullptr;

		return *this;
	}

	template<typename CharT>
	inline bool BB::Basic_String<CharT>::operator==(const Basic_String<CharT>& a_Rhs) const
	{
		if (Memory::Compare(m_String, a_Rhs.data(), m_Size) == 0)
			return true;
		return false;
	}

	template<typename CharT>
	inline void BB::Basic_String<CharT>::append(const Basic_String<CharT>& a_String)
	{
		append(a_String.c_str(), a_String.size());
	}

	template<typename CharT>
	inline void BB::Basic_String<CharT>::append(const Basic_String<CharT>& a_String, size_t a_SubPos, size_t a_SubLength)
	{
		append(a_String.c_str() + a_SubPos, a_SubLength);
	}

	template<typename CharT>
	inline void BB::Basic_String<CharT>::append(const CharT* a_String)
	{
		append(a_String, Memory::StrLength(a_String));
	}

	template<typename CharT>
	inline void BB::Basic_String<CharT>::append(const CharT* a_String, size_t a_Size)
	{
		if (m_Size + 1 + a_Size >= m_Capacity)
			grow(a_Size + 1);

		BB::Memory::Copy(m_String + m_Size, a_String, a_Size);
		m_Size += a_Size;
	}

	template<typename CharT>
	inline void BB::Basic_String<CharT>::insert(size_t a_Pos, const Basic_String<CharT>& a_String)
	{
		insert(a_Pos, a_String.c_str(), a_String.size());
	}

	template<typename CharT>
	inline void BB::Basic_String<CharT>::insert(size_t a_Pos, const Basic_String<CharT>& a_String, size_t a_SubPos, size_t a_SubLength)
	{
		insert(a_Pos, a_String.c_str() + a_SubPos, a_SubLength);
	}

	template<typename CharT>
	inline void BB::Basic_String<CharT>::insert(size_t a_Pos, const CharT* a_String)
	{
		insert(a_Pos, a_String, Memory::StrLength(a_String));
	}

	template<typename CharT>
	inline void BB::Basic_String<CharT>::insert(size_t a_Pos, const CharT* a_String, size_t a_Size)
	{
		BB_ASSERT(m_Size >= a_Pos, "String::Insert, trying to insert a string in a invalid position.");

		if (m_Size + 1 + a_Size >= m_Capacity)
			grow(a_Size + 1);

		Memory::sMove(m_String + (a_Pos + a_Size), m_String + a_Pos, m_Size - a_Pos);

		Memory::Copy(m_String + a_Pos, a_String, a_Size);
		m_Size += a_Size;
	}

	template<typename CharT>
	inline void BB::Basic_String<CharT>::push_back(const CharT a_Char)
	{
		if (m_Size + 1 >= m_Capacity)
			grow();

		m_String[m_Size++] = a_Char;
	}

	template<typename CharT>
	inline void BB::Basic_String<CharT>::pop_back()
	{
		m_String[m_Size--] = NULL;
	}

	template<typename CharT>
	inline bool BB::Basic_String<CharT>::compare(const Basic_String<CharT>& a_String) const
	{
		if (Memory::Compare(m_String, a_String.data(), m_Size) == 0)
			return true;
		return false;
	}

	template<typename CharT>
	inline bool BB::Basic_String<CharT>::compare(const Basic_String<CharT>& a_String, size_t a_Size) const
	{
		if (Memory::Compare(m_String, a_String.c_str(), a_Size) == 0)
			return true;
		return false;
	}

	template<typename CharT>
	inline bool BB::Basic_String<CharT>::compare(size_t a_Pos, const Basic_String<CharT>& a_String, size_t a_Subpos, size_t a_Size) const
	{
		if (Memory::Compare(m_String + a_Pos, a_String.c_str() + a_Subpos, a_Size) == 0)
			return true;
		return false;
	}

	template<typename CharT>
	inline bool BB::Basic_String<CharT>::compare(const CharT* a_String) const
	{
		return compare(a_String, Memory::StrLength(a_String));
	}

	template<typename CharT>
	inline bool BB::Basic_String<CharT>::compare(const CharT* a_String, size_t a_Size) const
	{
		if (Memory::Compare(m_String, a_String, a_Size) == 0)
			return true;
		return false;
	}

	template<typename CharT>
	inline bool BB::Basic_String<CharT>::compare(size_t a_Pos, const CharT* a_String) const
	{
		return compare(a_Pos, a_String, Memory::StrLength(a_String));
	}

	template<typename CharT>
	inline bool BB::Basic_String<CharT>::compare(size_t a_Pos, const CharT* a_String, size_t a_Size) const
	{
		if (Memory::Compare(m_String + a_Pos, a_String, a_Size) == 0)
			return true;
		return false;
	}

	template<typename CharT>
	inline void BB::Basic_String<CharT>::clear()
	{
		Memory::Set(m_String, NULL, m_Size);
		m_Size = 0;
	}

	template<typename CharT>
	inline void BB::Basic_String<CharT>::reserve(const size_t a_Size)
	{
		if (a_Size > m_Capacity)
		{
			size_t t_ModifiedCapacity = Math::RoundUp(a_Size + 1, String_Specs::multipleValue);

			reallocate(t_ModifiedCapacity);
		}
	}

	template<typename CharT>
	inline void BB::Basic_String<CharT>::shrink_to_fit()
	{
		size_t t_ModifiedCapacity = Math::RoundUp(m_Size + 1, String_Specs::multipleValue);
		if (t_ModifiedCapacity < m_Capacity)
		{
			reallocate(t_ModifiedCapacity);
		}
	}

	template<typename CharT>
	inline void BB::Basic_String<CharT>::grow(size_t a_MinCapacity)
	{
		size_t t_ModifiedCapacity = m_Capacity * 2;

		if (a_MinCapacity > t_ModifiedCapacity)
			t_ModifiedCapacity = Math::RoundUp(a_MinCapacity, String_Specs::multipleValue);

		reallocate(t_ModifiedCapacity);
	}

	template<typename CharT>
	inline void BB::Basic_String<CharT>::reallocate(size_t a_NewCapacity)
	{
		CharT* t_NewString = reinterpret_cast<CharT*>(BBalloc(m_Allocator, a_NewCapacity * sizeof(CharT)));

		Memory::Copy(t_NewString, m_String, m_Size);
		BBfree(m_Allocator, m_String);

		m_String = t_NewString;
		m_Capacity = a_NewCapacity;
	}


	template<typename CharT, size_t stringSize>
	class Stack_String
	{
	public:
		Stack_String()
		{
			Memory::Set(m_String, 0, stringSize);
		};
		Stack_String(const CharT* a_String) : Stack_String(a_String, Memory::StrLength(a_String)) {};
		Stack_String(const CharT* a_String, size_t a_Size)
		{
			BB_ASSERT(a_Size < sizeof(m_String), "Stack string overflow");
			Memory::Set(m_String, 0, sizeof(m_String));
			Memory::Copy(m_String, a_String, a_Size);
			m_Size = a_Size;
		};
		Stack_String(const Stack_String<CharT, stringSize>& a_String)
		{
			Memory::Copy(m_String, a_String, sizeof(m_String));
			m_Size = a_String.size;
		};
		Stack_String(Stack_String<CharT, stringSize>&& a_String) noexcept
		{
			Memory::Copy(m_String, a_String, sizeof(m_String));
			m_Size = a_String.size();

			Memory::Set(a_String.m_String, 0, stringSize);
			a_String.m_Size = 0
		};
		~Stack_String()
		{
			clear();
		};

		Stack_String& operator=(const Stack_String<CharT, stringSize>& a_Rhs)
		{
			this->~Stack_String();

			Memory::Copy(m_String, a_String, sizeof(m_String));
			m_Size = a_String.size();
		};
		Stack_String& operator=(Stack_String<CharT, stringSize>&& a_Rhs) noexcept
		{
			this->~Stack_String();

			Memory::Copy(m_String, a_String, sizeof(m_String));
			m_Size = a_String.size();

			Memory::Set(a_Rhs.m_String, 0, stringSize);
			a_Rhs.m_Size = 0
		};
		bool operator==(const Stack_String<CharT, stringSize>& a_Rhs) const
		{
			if (Memory::Compare(m_String, a_Rhs.data(), sizeof(m_String)) == 0)
				return true;
			return false;
		};

		void append(const Stack_String<CharT, stringSize>& a_String)
		{
			append(a_String.c_str(), a_String.size());
		};
		void append(const Stack_String<CharT, stringSize>& a_String, size_t a_SubPos, size_t a_SubLength)
		{
			append(a_String.c_str() + a_SubPos, a_SubLength);
		};
		void append(const CharT* a_String)
		{
			append(a_String, Memory::StrLength(a_String));
		};
		void append(const CharT* a_String, size_t a_Size)
		{
			BB_ASSERT(m_Size + a_Size < sizeof(m_String), "Stack string overflow");
			BB::Memory::Copy(m_String + m_Size, a_String, a_Size);
			m_Size += a_Size;
		};
		void insert(size_t a_Pos, const Stack_String<CharT, stringSize>& a_String)
		{
			insert(a_Pos, a_String.c_str(), a_String.size());
		};
		void insert(size_t a_Pos, const Stack_String<CharT, stringSize>& a_String, size_t a_SubPos, size_t a_SubLength)
		{
			insert(a_Pos, a_String.c_str() + a_SubPos, a_SubLength);
		}
		void insert(size_t a_Pos, const CharT* a_String)
		{
			insert(a_Pos, a_String, Memory::StrLength(a_String));
		};
		void insert(size_t a_Pos, const CharT* a_String, size_t a_Size)
		{
			BB_ASSERT(m_Size >= a_Pos, "Trying to insert a string in a invalid position.");
			BB_ASSERT(m_Size + a_Size < sizeof(m_String), "Stack string overflow");

			Memory::sMove(m_String + (a_Pos + a_Size), m_String + a_Pos, m_Size - a_Pos);

			Memory::Copy(m_String + a_Pos, a_String, a_Size);
			m_Size += a_Size;
		};
		void push_back(const CharT a_Char)
		{
			m_String[m_Size++] = a_Char;
			BB_ASSERT(m_Size < sizeof(m_String), "Stack string overflow");
		};

		void pop_back(uint32_t a_Count)
		{
			m_Size -= a_Count;
			memset(Pointer::Add(m_String, m_Size), NULL, a_Count);
		};

		void clear()
		{
			Memory::Set(m_String, 0, stringSize);
			m_Size = 0;
		}

		size_t size() const { return m_Size; }
		size_t capacity() const { return stringSize; }
		CharT* data() { return m_String; }
		const CharT* c_str() const { return m_String; }

	private:
		CharT m_String[stringSize + 1];
		size_t m_Size = 0;
	};

	template<size_t stringSize>
	using StackString = Stack_String<char, stringSize>;
	template<size_t stringSize>
	using StackWString = Stack_String<wchar_t, stringSize>;
}