#pragma once
#pragma warning (push, 0)
#include <gtest/gtest.h>
#pragma warning (pop)
#include "Utils/Utils.h"

//Structs with different sizes, union to check for the values.
struct size32Bytes { union { char data[32]; size_t value; }; };
struct size256Bytes { union { char data[256]; size_t value; }; };
struct size2593bytes { union { char data[2593]; size_t value; }; };

//Unaligned big struct with a union to test the value.
struct size2593bytesObj
{
	size2593bytesObj() {};
	size2593bytesObj(const size_t a_Value) : value(a_Value) {};
	size2593bytesObj(const size2593bytesObj& a_Rhs)
	{
		value = a_Rhs.value;
		memcpy(data, a_Rhs.data, sizeof(data));
	};
	size2593bytesObj(size2593bytesObj& a_Rhs)
	{
		value = a_Rhs.value;
		memcpy(data, a_Rhs.data, sizeof(data));
	};
	size2593bytesObj& operator=(const size2593bytesObj& a_Rhs)
	{
		value = a_Rhs.value;
		memcpy(data, a_Rhs.data, sizeof(data));

		return *this;
	};
	~size2593bytesObj() { value = 0; };

	union { char data[2593]; size_t value; };
};