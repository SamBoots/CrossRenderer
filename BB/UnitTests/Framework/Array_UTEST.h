#pragma once
#include "../TestValues.h"
#include "Storage/Array.h"

TEST(ArrayDataStructure, push_reserve)
{
	constexpr const size_t initialSize = 8;
	constexpr const size_t samples = initialSize * 16;

	//32 MB alloactor.
	const size_t allocatorSize = BB::mbSize * 32;
	BB::FreelistAllocator_t t_Allocator(allocatorSize);

	BB::Array<size2593bytes> t_Array(t_Allocator);
	EXPECT_EQ(t_Array.capacity(), BB::Array_Specs::multipleValue);

	//Allocate an object without having allocated memory, this must be valid.
	{
		constexpr const size_t testvalue = 123456;

		size2593bytes t_Object{};
		t_Object.value = testvalue;
		t_Array.push_back(t_Object);
		EXPECT_EQ(t_Array[0].value, testvalue);
		//Real test starts now, so remove latest pushed variable.
		t_Array.pop();
	}
	size_t t_OldCapacity = t_Array.capacity();

	EXPECT_NE(t_OldCapacity, initialSize * 16) << "Just allocating one element seems to be enough for the test, this is wrong and might indicate an unaccurate test.";
	t_Array.reserve(initialSize);
	//Because of overallocationmultiplier this should reserve some.
	EXPECT_NE(t_OldCapacity, initialSize * 16) << "Reserve overallocates while reserve should be able to do that!";

	size_t t_RandomValues[samples]{};

	for (size_t i = 0; i < samples; i++)
	{
		t_RandomValues[i] = static_cast<size_t>(BB::Random::Random());
	}

	//Cache the current capacity since we will go over it. 
	t_OldCapacity = t_Array.capacity();
	for (size_t i = 0; i <  initialSize; i++)
	{
		size2593bytes t_Object{};
		t_Object.value = t_RandomValues[i];
		t_Array.push_back(t_Object);
	};

	//Test all results before doing a resize event
	for (size_t i = 0; i < t_Array.size(); i++)
	{
		EXPECT_EQ(t_Array[i].value, t_RandomValues[i]) << "Array value is wrong, something went bad before a resize event.";
	}
	

	EXPECT_EQ(t_OldCapacity, t_Array.capacity()) << "Array resized while it shouldn't!";
	
	//add a single object to go over the limit and test the resize.
	size2593bytes t_LimitObject{};
	t_LimitObject.value = t_RandomValues[t_Array.size()];
	t_Array.push_back(t_LimitObject);

	t_OldCapacity = t_Array.capacity();
	EXPECT_EQ(t_Array[t_Array.size() - 1].value, t_LimitObject.value) << "Array resize event went badly.";

	//Empty the array to prepare for the next test.
	t_Array.clear();

	//Reserve enough for the entire test.
	t_Array.reserve(samples);

	EXPECT_EQ(t_Array.capacity(), samples) << "Array's multiple seems to be changed or bad, this might indicate an unaccurate test.";

	//cache capacity so that we can compare if more was needed later on.
	t_OldCapacity = t_Array.capacity();

	//now just fill the entire thing up.
	for (size_t i = 0; i < samples; i++)
	{
		size2593bytes t_Object{};
		t_Object.value = t_RandomValues[i];
		t_Array.push_back(t_Object);
	};

	for (size_t i = 0; i < samples; i++)
	{
		EXPECT_EQ(t_Array[i].value, t_RandomValues[i]) << "Array value is wrong, something went bad after the resize events.";
	}

	EXPECT_EQ(t_OldCapacity, t_Array.capacity()) << "Array capacity has been resized at the end while enough should've been reserve.";
}

TEST(ArrayDataStructure, Array_push)
{
	constexpr const size_t initialSize = 8;
	constexpr const size_t pushSize = 64;
	//Unaligned big struct with a union to test the value.
	struct size2593bytes { union { char data[2593]; size_t value; }; };

	//2 MB alloactor.
	BB::FreelistAllocator_t t_Allocator(1024 * 1024 * 2);

	BB::Array<size2593bytes> t_Array(t_Allocator, initialSize);

	size_t t_RandomValues[pushSize]{};
	size2593bytes t_SizeArray[initialSize]{};

	for (size_t i = 0; i < pushSize; i++)
	{
		t_RandomValues[i] = static_cast<size_t>(BB::Random::Random());
	}

	for (size_t i = 0; i < initialSize; i++)
	{
		t_SizeArray[i].value = t_RandomValues[i];
	}
	//Cache the current capacity since need to check that we do not go over it. 
	size_t t_OldCapacity = t_Array.capacity();
	t_Array.push_back(t_SizeArray, initialSize);
	EXPECT_EQ(t_OldCapacity, t_Array.capacity()) << "Array capacity has been resized while enough should've been reserved.";

	for (size_t i = 0; i < initialSize; i++)
	{
		EXPECT_EQ(t_Array[i].value, t_RandomValues[i]) << "Array, first array test has wrong values.";
	}
	
	//Empty for next task.
	t_Array.clear();

	size2593bytes t_SizeArray2[pushSize]{};
	for (size_t i = 0; i < pushSize; i++)
	{
		t_SizeArray2[i].value = t_RandomValues[i];
	}
	//Cache the current capacity since need to check that we do not go over it. 
	t_OldCapacity = t_Array.capacity();
	t_Array.push_back(t_SizeArray2, pushSize);
	EXPECT_NE(t_OldCapacity, t_Array.capacity()) << "Array capacity is not different from the previous one, too much was allocated.";

	for (size_t i = 0; i < pushSize; i++)
	{
		EXPECT_EQ(t_SizeArray2[i].value, t_RandomValues[i]) << "Array, second array test has wrong values.";
	}

	//let it grow by one and compare all the values again.
	size2593bytes t_SingleElement{};
	t_SingleElement.value = t_RandomValues[t_Array.size()];
	t_Array.push_back(t_SingleElement);

	EXPECT_NE(t_OldCapacity, t_Array.capacity()) << "Array capacity is not different from the previous one, too much was allocated.";

	for (size_t i = 0; i < pushSize; i++)
	{
		EXPECT_EQ(t_SizeArray2[i].value, t_RandomValues[i]) << "Array, second array test has wrong values.";
	}
}


TEST(ArrayDataStructure, Array_emplace_back)
{
	constexpr const size_t initialSize = 8;
	constexpr const size_t samples = initialSize;
	//Unaligned big struct with a union to test the value.
	struct size2593bytes { 
		size2593bytes() { value = 0; }
		size2593bytes(size_t a_Value) : value(a_Value) {}
		union { char data[2593]; size_t value; }; 
	};

	//32 MB alloactor.
	const size_t allocatorSize = BB::mbSize * 32;
	BB::FreelistAllocator_t t_Allocator(allocatorSize);

	BB::Array<size2593bytes> t_Array(t_Allocator);
	EXPECT_EQ(t_Array.capacity(), BB::Array_Specs::multipleValue);

	size_t t_RandomValues[samples]{};

	for (size_t i = 0; i < samples; i++)
	{
		t_RandomValues[i] = static_cast<size_t>(BB::Random::Random());
	}

	//Cache the current capacity since we will go over it. 
	for (size_t i = 0; i < initialSize; i++)
	{
		t_Array.emplace_back(t_RandomValues[i]);
	};

	//Test all results before doing a resize event
	for (size_t i = 0; i < t_Array.size(); i++)
	{
		EXPECT_EQ(t_Array[i].value, t_RandomValues[i]) << "Array value is wrong, something went bad before a resize event.";
	}
}

TEST(ArrayDataStructure, Array_insert_single)
{
	constexpr const size_t initialSize = 8;
	constexpr const size_t randomNumberSize = 16; //Get double to test insertion
	//Unaligned big struct with a union to test the value.
	struct size2593bytes { union { char data[2593]; size_t value; }; };

	//2 MB alloactor.
	BB::FreelistAllocator_t t_Allocator(1024 * 1024 * 2);

	BB::Array<size2593bytes> t_Array(t_Allocator, initialSize);

	size_t t_RandomValues[randomNumberSize]{};

	size2593bytes t_SizeArray[randomNumberSize]{};

	for (size_t i = 0; i < randomNumberSize; i++)
	{
		t_RandomValues[i] = static_cast<size_t>(BB::Random::Random());
	}

	for (size_t i = 0; i < randomNumberSize; i++)
	{
		t_SizeArray[i].value = t_RandomValues[i];
	}

	//Cache the current capacity since need to check that we do not go over it. 
	size_t t_OldCapacity = t_Array.capacity();

	for (size_t i = 0; i < initialSize; i++)
	{
		t_Array.insert(i, t_SizeArray[i]);
	}

	EXPECT_EQ(t_OldCapacity, t_Array.capacity()) << "Array capacity has been resized while enough should've been reserved.";

	for (size_t i = 0; i < initialSize; i++)
	{
		EXPECT_EQ(t_Array[i].value, t_RandomValues[i]) << "Array, first array test has wrong values.";
	}

	//Now insert from the buttom again. But take the next 8 random numbers
	for (size_t i = 0; i < initialSize; i++)
	{
		t_Array.insert(i, t_SizeArray[i + initialSize]);
	}
	//Now check the begin 8 values that we just placed.
	for (size_t i = 0; i < initialSize; i++)
	{
		EXPECT_EQ(t_Array[i].value, t_RandomValues[i + initialSize]) << "Array, begin 8 array test has wrong values.";
	}

	//Now check the back 8.
	for (size_t i = 0; i < initialSize; i++)
	{
		EXPECT_EQ(t_Array[i + initialSize].value, t_RandomValues[i]) << "Array, back 8 array test has wrong values.";
	}
}

TEST(ArrayDataStructure, Array_Object_Test)
{
	constexpr const size_t initialSize = 8;
	constexpr const size_t randomNumberSize = 16; //Get double to test insertion
	//Unaligned big struct with a union to test the value.
	struct size2593bytesObj
	{
		size2593bytesObj() { value = 0; };
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

	//2 MB alloactor.
	BB::FreelistAllocator_t t_Allocator(1024 * 1024 * 2);

	BB::Array<size2593bytesObj> t_Array(t_Allocator, initialSize);

	size_t t_RandomValues[randomNumberSize]{};

	size2593bytesObj t_SizeArray[randomNumberSize]{};

	for (size_t i = 0; i < randomNumberSize; i++)
	{
		t_RandomValues[i] = static_cast<size_t>(BB::Random::Random());
	}

	for (size_t i = 0; i < randomNumberSize; i++)
	{
		t_SizeArray[i].value = t_RandomValues[i];
	}

	//Cache the current capacity since need to check that we do not go over it. 
	size_t t_OldCapacity = t_Array.capacity();

	for (size_t i = 0; i < initialSize; i++)
	{
		t_Array.insert(i, t_SizeArray[i]);
	}

	EXPECT_EQ(t_OldCapacity, t_Array.capacity()) << "Array capacity has been resized while enough should've been reserved.";

	for (size_t i = 0; i < initialSize; i++)
	{
		EXPECT_EQ(t_Array[i].value, t_RandomValues[i]) << "Array, first array test has wrong values.";
	}

	//Now insert from the buttom again. But take the next 8 random numbers
	for (size_t i = 0; i < initialSize; i++)
	{
		t_Array.insert(i, t_SizeArray[i + initialSize]);
	}
	//Now check the begin 8 values that we just placed.
	for (size_t i = 0; i < initialSize; i++)
	{
		EXPECT_EQ(t_Array[i].value, t_RandomValues[i + initialSize]) << "Array, begin 8 array test has wrong values.";
	}

	//Now check the back 8.
	for (size_t i = 0; i < initialSize; i++)
	{
		EXPECT_EQ(t_Array[i + initialSize].value, t_RandomValues[i]) << "Array, back 8 array test has wrong values.";
	}
}

TEST(ArrayDataStructure, Array_Copy)
{
	constexpr const size_t testSize = 18;
	//Unaligned big struct with a union to test the value.
	struct size2593bytes { union { char data[2593]; size_t value; }; };

	//2 MB alloactor.
	BB::FreelistAllocator_t t_Allocator(1024 * 1024 * 2);

	BB::Array<size2593bytes> t_Array(t_Allocator, testSize);

	size_t t_RandomValues[testSize]{};

	size2593bytes t_SizeArray[testSize]{};

	for (size_t i = 0; i < testSize; i++)
	{
		t_RandomValues[i] = static_cast<size_t>(BB::Random::Random());
	}

	for (size_t i = 0; i < testSize; i++)
	{
		t_SizeArray[i].value = t_RandomValues[i];
	}

	t_Array.push_back(t_SizeArray, testSize);

	BB::Array<size2593bytes> t_CopyArray(t_Array);

	for (size_t i = 0; i < testSize; i++)
	{
		EXPECT_EQ(t_CopyArray[i].value, t_RandomValues[i]) << "Array, copy array test has wrong values.";
	}

	BB::Array<size2593bytes> t_CopyOperatorArray(t_Allocator);
	t_CopyOperatorArray = t_Array;

	for (size_t i = 0; i < testSize; i++)
	{
		EXPECT_EQ(t_CopyOperatorArray[i].value, t_Array[i].value) << "Array, copy operator array test has wrong values.";
	}
}


TEST(ArrayDataStructure, Array_Assignment)
{
	constexpr const size_t testSize = 18;
	//Unaligned big struct with a union to test the value.
	struct size2593bytes { union { char data[2593]; size_t value; }; };

	//2 MB alloactor.
	BB::FreelistAllocator_t t_Allocator(1024 * 1024 * 2);

	BB::Array<size2593bytes> t_Array(t_Allocator, testSize);

	size_t t_RandomValues[testSize]{};

	size2593bytes t_SizeArray[testSize]{};

	for (size_t i = 0; i < testSize; i++)
	{
		t_RandomValues[i] = static_cast<size_t>(BB::Random::Random());
	}

	for (size_t i = 0; i < testSize; i++)
	{
		t_SizeArray[i].value = t_RandomValues[i];
	}

	t_Array.push_back(t_SizeArray, testSize);

	BB::Array<size2593bytes> t_AssignmentArray(std::move(t_Array));

	EXPECT_EQ(t_Array.data(), nullptr) << "Array, old array that was used in assignment operator still has data.";

	for (size_t i = 0; i < testSize; i++)
	{
		EXPECT_EQ(t_AssignmentArray[i].value, t_RandomValues[i]) << "Array, assignment array test has wrong values.";
	}

	BB::Array<size2593bytes> t_AssignmentOperatorArray(t_Allocator);
	t_AssignmentOperatorArray = std::move(t_AssignmentArray);

	EXPECT_EQ(t_AssignmentArray.data(), nullptr) << "Array, old array that was used in assignment operator still has data.";

	for (size_t i = 0; i < testSize; i++)
	{
		EXPECT_EQ(t_AssignmentOperatorArray[i].value, t_RandomValues[i]) << "Array, assignment operator array test has wrong values.";
	}
}