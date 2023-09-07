#pragma once
#include "Utils/Logger.h"
namespace BB
{
	template<typename T, size_t arrSize>
	struct FixedArray
	{
		T& operator[](const size_t a_Index);

		constexpr size_t size() const { return arrSize; };
		T* data() { return m_Arr; };

		T m_Arr[arrSize]{};
	};

	template<typename T, size_t arrSize>
	inline T& FixedArray<T, arrSize>::operator[](const size_t a_Index)
	{
		BB_ASSERT(a_Index <= arrSize, "FixedArray, trying to access a index that is out of bounds.");
		return m_Arr[a_Index];
	}
}