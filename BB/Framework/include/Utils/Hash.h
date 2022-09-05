#pragma once
#include "Utils/Logger.h"
#include <type_traits>

struct Hash
{
	Hash() {};
	Hash(uint64_t a_Hash) : hash(a_Hash) {};
	uint64_t hash = 0;

	operator const uint64_t() const { return hash; }
	void operator=(const uint64_t a_Rhs) { hash = a_Rhs; }
	Hash operator++(int) { return hash++; }
	void operator*=(size_t a_Multi) { hash *= a_Multi; }

	//Create with uint64_t.
	template<typename T>
	static Hash MakeHash(T a_Value);
	template <size_t>
	static Hash MakeHash(size_t a_Value);
	template <const char*>
	static Hash MakeHash(const char* a_Value);

private:

};

template<typename T>
inline Hash Hash::MakeHash(T a_Value)
{
	constexpr bool is_integral_v = std::is_integral<T>::value;
	if (is_integral_v)
		return MakeHash<size_t>(a_Value);;

	BB_ASSERT(false, "MakeHash function doesn't support this type");
	return Hash();
}

template<>
inline Hash Hash::MakeHash(size_t a_Value)
{
	a_Value ^= a_Value << 13, a_Value ^= a_Value >> 17;
	a_Value ^= a_Value << 5;
	return Hash(a_Value);
}

//template<>
//inline Hash Hash::MakeHash(uint32_t a_Value)
//{
//	a_Value ^= a_Value << 13, a_Value ^= a_Value >> 17;
//	a_Value ^= a_Value << 5;
//	return Hash(a_Value);
//}

template<>
inline Hash Hash::MakeHash(const char* a_Value)
{
	uint64_t t_Hash = 0;

	while (*a_Value != '\0')
	{
		t_Hash += static_cast<uint64_t>(*a_Value++);
	}

	return Hash(t_Hash);
}