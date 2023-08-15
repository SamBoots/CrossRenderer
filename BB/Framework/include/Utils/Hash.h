#pragma once
#include "Utils/Logger.h"
#include <type_traits>

//will remove this, I don't like it.
//Maybe a unified hash is cringe and I should just have some basic hashing operations in this file.
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
	static Hash MakeHash(size_t a_Value);
	static Hash MakeHash(const char* a_Value);
	static Hash MakeHash(void* a_Value);

private:

};

inline Hash Hash::MakeHash(size_t a_Value)
{
	a_Value ^= a_Value << 13, a_Value ^= a_Value >> 17;
	a_Value ^= a_Value << 5;
	return Hash(a_Value);
}

inline Hash Hash::MakeHash(void* a_Value)
{
	uintptr_t t_Value = reinterpret_cast<uintptr_t>(a_Value);

	t_Value ^= t_Value << 13, t_Value ^= t_Value >> 17;
	t_Value ^= t_Value << 5;
	return Hash(t_Value);
}

inline Hash Hash::MakeHash(const char* a_Value)
{
	uint64_t t_Hash = 5381;
	int t_C;

	while (t_C = *a_Value++)
		t_Hash = ((t_Hash << 5) + t_Hash) + t_C;

	return Hash(t_Hash);
}