#pragma once
#include "../TestValues.h"
#include "Utils/Slice.h"

TEST(Slice_Utils, Slice_Utils_Boundry_Check)
{
	constexpr const size_t arraySize = 5;
	//Unaligned big struct with a union to test the value.
	struct size2593bytes { union { char data[2593]; size_t value; }; };

	size2593bytes t_Array[arraySize]{};
	BB::Slice t_Slice(t_Array, arraySize);

	for (size_t i = 0; i < arraySize; i++)
	{
		//Just access it and modify the value to 1, testing asserts.
		t_Slice[i].value = 5;
	}

	for (size_t i = 0; i < arraySize; i++)
	{
		EXPECT_EQ(t_Array[i].value, 5);
	}

	for (size_t i = 0; i < arraySize; i++)
	{
		BB::Slice t_TempSlice = t_Slice.SubSlice(i, 1);
		t_TempSlice[0].value = 12;
	}

	for (size_t i = 0; i < arraySize; i++)
	{
		EXPECT_EQ(t_Array[i].value, 12);
	}
}