#pragma once
#include "../TestValues.h"
#include "Allocators/AllocTypes.h"

//Bytes samples with different sizes.
constexpr const size_t sample_32_bytes = 10000;
constexpr const size_t sample_256_bytes = 2000;
constexpr const size_t sample_2593_bytes = 500;

//How many samples in total.
constexpr const size_t samples = sample_32_bytes + sample_256_bytes + sample_2593_bytes;

#pragma region LINEAR_ALLOCATOR
TEST(MemoryAllocators, LINEAR_SINGLE_ALLOCATIONS)
{
	std::cout << "Linear allocator with " 
		<< sample_32_bytes << " 32 byte samples, " 
		<< sample_256_bytes << " 256 byte samples and "
		<< sample_2593_bytes << " 2593 bytes samples." << "\n";

	constexpr const size_t allocatorSize = 
		sizeof(size32Bytes) * sample_32_bytes +
		sizeof(size256Bytes) * sample_256_bytes +
		sizeof(size2593bytes) * sample_2593_bytes;

	//Get some random values to test.
	size_t randomValues[samples]{};
	for (size_t i = 0; i < samples; i++)
	{
		randomValues[i] = static_cast<size_t>(BB::Random::Random());
	}

	BB::LinearAllocator_t t_LinearAllocator(allocatorSize);
	
	for (size_t i = 0; i < sample_32_bytes; i++)
	{
		size32Bytes* sample = BB::BBnew<size32Bytes>(t_LinearAllocator);
		sample->value = randomValues[i];
	}
	for (size_t i = 0; i < sample_256_bytes; i++)
	{
		size256Bytes* sample = BB::BBnew<size256Bytes>(t_LinearAllocator);
		sample->value = randomValues[sample_32_bytes + i];
	}
	for (size_t i = 0; i < sample_2593_bytes; i++)
	{
		size2593bytes* sample = BB::BBnew<size2593bytes>(t_LinearAllocator);
		sample->value = randomValues[sample_32_bytes + sample_256_bytes + i];
	}

	////Test is depricated because of Boundrychecking.
	//Test all the values inside the allocations
	//void* t_AllocData = t_LinearAllocator.begin();
	//for (size_t i = 0; i < sample_32_bytes; i++)
	//{
	//	size32Bytes* data = reinterpret_cast<size32Bytes*>(t_AllocData);
	//	ASSERT_EQ(data->value, randomValues[i]) << "32 bytes, Value is different in the linear allocator.";
	//	t_AllocData = BB::pointerutils::Add(t_AllocData, sizeof(size32Bytes));
	//}
	//for (size_t i = sample_32_bytes; i < sample_32_bytes + sample_256_bytes; i++)
	//{
	//	ASSERT_EQ(reinterpret_cast<size256Bytes*>(t_AllocData)->value, randomValues[i]) << "256 bytes, Value is different in the linear allocator.";
	//	t_AllocData = BB::pointerutils::Add(t_AllocData, sizeof(size256Bytes));
	//}
	//for (size_t i = sample_32_bytes + sample_256_bytes; i < sample_32_bytes + sample_256_bytes + sample_2593_bytes; i++)
	//{
	//	ASSERT_EQ(reinterpret_cast<size2593bytes*>(t_AllocData)->value, randomValues[i]) << "2593 bytes, Value is different in the linear allocator.";
	//	t_AllocData = BB::pointerutils::Add(t_AllocData, sizeof(size2593bytes));
	//}

	t_LinearAllocator.Clear();
}


TEST(MemoryAllocators, LINEAR_ARRAY_ALLOCATIONS)
{
	std::cout << "Linear allocator with "
		<< sample_32_bytes << " 32 byte samples, "
		<< sample_256_bytes << " 256 byte samples and "
		<< sample_2593_bytes << " 2593 bytes samples." << "\n";

	constexpr const size_t allocatorSize =
		sizeof(size32Bytes) * sample_32_bytes +
		sizeof(size256Bytes) * sample_256_bytes +
		sizeof(size2593bytes) * sample_2593_bytes;

	//Get some random values to test.
	size_t randomValues[samples]{};
	for (size_t i = 0; i < samples; i++)
	{
		randomValues[i] = static_cast<size_t>(BB::Random::Random());
	}

	BB::LinearAllocator_t t_LinearAllocator(allocatorSize);

	size32Bytes* size32Array = BB::BBnewArr<size32Bytes>(t_LinearAllocator, sample_32_bytes);
	size256Bytes* size256Array = BB::BBnewArr<size256Bytes>(t_LinearAllocator, sample_256_bytes);
	size2593bytes* size2593Array = BB::BBnewArr<size2593bytes>(t_LinearAllocator, sample_2593_bytes);
	
	//Checking the arrays
	for (size_t i = 0; i < sample_32_bytes; i++)
	{
		size32Array[i].value = randomValues[i];
	}
	for (size_t i = 0; i < sample_256_bytes; i++)
	{
		size256Array[i].value = randomValues[sample_32_bytes + i];
	}
	for (size_t i = 0; i < sample_2593_bytes; i++)
	{
		size2593Array[i].value = randomValues[sample_32_bytes + sample_256_bytes + i];
	}

	//Checking the arrays
	for (size_t i = 0; i < sample_32_bytes; i++)
	{
		ASSERT_EQ(size32Array[i].value, randomValues[i]) << "32 bytes, Value is different in the linear allocator.";
	}
	for (size_t i = 0; i < sample_256_bytes; i++)
	{
		ASSERT_EQ(size256Array[i].value, randomValues[sample_32_bytes + i]) << "256 bytes, Value is different in the linear allocator.";
	}
	for (size_t i = 0; i < sample_2593_bytes; i++)
	{
		ASSERT_EQ(size2593Array[i].value, randomValues[sample_32_bytes + sample_256_bytes + i]) << "2593 bytes, Value is different in the linear allocator.";
	}

	t_LinearAllocator.Clear();
}
#pragma endregion

#pragma region FREELIST_ALLOCATOR
TEST(MemoryAllocators, FREELIST_SINGLE_ALLOCATIONS)
{
	std::cout << "Freelist allocator with "
		<< sample_32_bytes << " 32 byte samples, "
		<< sample_256_bytes << " 256 byte samples and "
		<< sample_2593_bytes << " 2593 bytes samples." << "\n";
	//allocator size is modified by the allocheader it needs.
	constexpr const size_t allocatorSize =
		(sizeof(size32Bytes) * sample_32_bytes +
		sizeof(size256Bytes) * sample_256_bytes +
		sizeof(size2593bytes) * sample_2593_bytes) * 
		sizeof(BB::allocators::FreelistAllocator::AllocHeader);

	//Get some random values to test.
	size_t randomValues[samples]{};
	for (size_t i = 0; i < samples; i++)
	{
		randomValues[i] = static_cast<size_t>(BB::Random::Random());
	}

	BB::FreeListAllocator_t t_FreelistAllocator(allocatorSize);

	{
		//This address should always be used since it's a free block.
		void* repeatAddress32 = BB::BBnew<size32Bytes>(t_FreelistAllocator);
		BB::BBfree(t_FreelistAllocator, repeatAddress32);

		for (size_t i = 0; i < sample_32_bytes; i++)
		{
			size32Bytes* sample = BB::BBnew<size32Bytes>(t_FreelistAllocator);
			sample->value = randomValues[i];
			//Compare the values.
			ASSERT_EQ(sample->value, randomValues[i]) << "32 bytes, Value is different in the freelist allocator.";
			//Compare the addresses.
			ASSERT_EQ(sample, repeatAddress32) << "32 bytes, address is different in the freelist allocator.";
			BB::BBfree(t_FreelistAllocator, sample);
		}
	}
	{
		//This address should always be used since it's a free block.
		void* repeatAddress256 = BB::BBnew<size256Bytes>(t_FreelistAllocator);
		BB::BBfree(t_FreelistAllocator, repeatAddress256);

		for (size_t i = 0; i < sample_256_bytes; i++)
		{
			size256Bytes* sample = BB::BBnew<size256Bytes>(t_FreelistAllocator);
			sample->value = randomValues[sample_32_bytes + i];
			//Compare the values.
			ASSERT_EQ(sample->value, randomValues[sample_32_bytes + i]) << "256 bytes, Value is different in the freelist allocator.";
			//Compare the addresses.
			ASSERT_EQ(sample, repeatAddress256) << "256 bytes, address is different in the freelist allocator.";
			BB::BBfree(t_FreelistAllocator, sample);
		}
	}
	{
		//This address should always be used since it's a free block.
		void* repeatAddress2593 = BB::BBnew<size2593bytes>(t_FreelistAllocator);
		BB::BBfree(t_FreelistAllocator, repeatAddress2593);

		for (size_t i = 0; i < sample_2593_bytes; i++)
		{
			size2593bytes* sample = BB::BBnew<size2593bytes>(t_FreelistAllocator);
			sample->value = randomValues[sample_32_bytes + sample_256_bytes + i];
			//Compare the values.
			ASSERT_EQ(sample->value, randomValues[sample_32_bytes + sample_256_bytes + i]) << "2593 bytes, Value is different in the freelist allocator.";
			//Compare the addresses.
			ASSERT_EQ(sample, repeatAddress2593) << "2593 bytes, address is different in the freelist allocator.";
			BB::BBfree(t_FreelistAllocator, sample);
		}
	}
	//Clear is not suppoted by freelist, commented just to show this is not a mistake.
	//t_FreelistAllocator.Clear();
}


TEST(MemoryAllocators, FREELIST_ARRAY_ALLOCATIONS)
{
	std::cout << "Freelist allocator with 10000 32 byte samples, 2000 256 byte samples and 1000 2593 bytes samples." << "\n";

	constexpr const size_t allocatorSize =
		(sizeof(size32Bytes) * sample_32_bytes +
			sizeof(size256Bytes) * sample_256_bytes +
			sizeof(size2593bytes) * sample_2593_bytes) * 2;

	//Get some random values to test.
	size_t randomValues[samples]{};
	for (size_t i = 0; i < samples; i++)
	{
		randomValues[i] = static_cast<size_t>(BB::Random::Random());
	}

	BB::FreeListAllocator_t t_FreeList(allocatorSize);

	size32Bytes* size32Array = BB::BBnewArr<size32Bytes>(t_FreeList, sample_32_bytes);
	size256Bytes* size256Array = BB::BBnewArr<size256Bytes>(t_FreeList, sample_256_bytes);
	size2593bytes* size2593Array = BB::BBnewArr<size2593bytes>(t_FreeList, sample_2593_bytes);

	//Checking the arrays
	for (size_t i = 0; i < sample_32_bytes; i++)
	{
		size32Array[i].value = randomValues[i];
	}
	for (size_t i = 0; i < sample_256_bytes; i++)
	{
		size256Array[i].value = randomValues[sample_32_bytes + i];
	}
	for (size_t i = 0; i < sample_2593_bytes; i++)
	{
		size2593Array[i].value = randomValues[sample_32_bytes + sample_256_bytes + i];
	}

	//Checking the arrays
	for (size_t i = 0; i < sample_32_bytes; i++)
	{
		ASSERT_EQ(size32Array[i].value, randomValues[i]) << "32 bytes, Value is different in the freelist allocator.";
	}
	BB::BBfreeArr(t_FreeList, size32Array);
	for (size_t i = 0; i < sample_256_bytes; i++)
	{
		ASSERT_EQ(size256Array[i].value, randomValues[sample_32_bytes + i]) << "256 bytes, Value is different in the freelist allocator.";
	}
	BB::BBfreeArr(t_FreeList, size256Array);
	for (size_t i = 0; i < sample_2593_bytes; i++)
	{
		ASSERT_EQ(size2593Array[i].value, randomValues[sample_32_bytes + sample_256_bytes + i]) << "2593 bytes, Value is different in the freelist allocator.";
	}
	BB::BBfreeArr(t_FreeList, size2593Array);
	//Clear is not suppoted by freelist, commented just to show this is not a mistake.
	//t_FreelistAllocator.Clear();
}

#pragma endregion

#pragma region POW_FreeList_ALLOCATOR

TEST(MemoryAllocators, POW_FREELIST_SINGLE_ALLOCATIONS)
{
	std::cout << "POW Freelist allocator with "
		<< sample_32_bytes << " 32 byte samples, "
		<< sample_256_bytes << " 256 byte samples and "
		<< sample_2593_bytes << " 2593 bytes samples." << "\n";

	//allocator size is modified by the allocheader it needs.
	constexpr const size_t allocatorSize =
		(sizeof(size32Bytes) * sample_32_bytes +
			sizeof(size256Bytes) * sample_256_bytes);

	//Get some random values to test.
	size_t randomValues[samples]{};
	for (size_t i = 0; i < samples; i++)
	{
		randomValues[i] = static_cast<size_t>(BB::Random::Random());
	}

	BB::POW_FreeListAllocator_t t_POW_FreelistAllocator(allocatorSize);

	{
		////This address should always be used since it's a free block.
		void* repeatAddress32 = BB::BBnew<size32Bytes>(t_POW_FreelistAllocator);
		BB::BBfree(t_POW_FreelistAllocator, repeatAddress32);

		for (size_t i = 0; i < sample_32_bytes; i++)
		{
			size32Bytes* sample = BB::BBnew<size32Bytes>(t_POW_FreelistAllocator);
			sample->value = randomValues[i];
			//Compare the values.
			ASSERT_EQ(sample->value, randomValues[i]) << "32 bytes, Value is different in the freelist allocator.";
			//Compare the addresses.
			ASSERT_EQ(sample, repeatAddress32) << "32 bytes, address is different in the freelist allocator.";
			BB::BBfree(t_POW_FreelistAllocator, sample);
		}
	}
	{
		////This address should always be used since it's a free block.
		void* repeatAddress256 = BB::BBnew<size256Bytes>(t_POW_FreelistAllocator);
		BB::BBfree(t_POW_FreelistAllocator, repeatAddress256);

		for (size_t i = 0; i < sample_256_bytes; i++)
		{
			size256Bytes* sample = BB::BBnew<size256Bytes>(t_POW_FreelistAllocator);
			sample->value = randomValues[sample_32_bytes + i];
			//Compare the values.
			ASSERT_EQ(sample->value, randomValues[sample_32_bytes + i]) << "256 bytes, Value is different in the freelist allocator.";
			//Compare the addresses.
			ASSERT_EQ(sample, repeatAddress256) << "256 bytes, address is different in the freelist allocator.";
			BB::BBfree(t_POW_FreelistAllocator, sample);
		}
	}

	{
		//This address should always be used since it's a free block.
		void* repeatAddress2593 = BB::BBnew<size2593bytes>(t_POW_FreelistAllocator);
		BB::BBfree(t_POW_FreelistAllocator, repeatAddress2593);

		for (size_t i = 0; i < sample_2593_bytes; i++)
		{
			size2593bytes* sample = BB::BBnew<size2593bytes>(t_POW_FreelistAllocator);
			sample->value = randomValues[sample_32_bytes + sample_256_bytes + i];
			//Compare the values.
			ASSERT_EQ(sample->value, randomValues[sample_32_bytes + sample_256_bytes + i]) << "2593 bytes, Value is different in the freelist allocator.";
			//Compare the addresses.
			ASSERT_EQ(sample, repeatAddress2593) << "2593 bytes, address is different in the freelist allocator.";
			BB::BBfree(t_POW_FreelistAllocator, sample);
		}
	}
}


//TEST(MemoryAllocators, POW_FREELIST_ARRAY_ALLOCATIONS)
//{
//	std::cout << "Freelist allocator with 10000 32 byte samples, 2000 256 byte samples and 1000 2593 bytes samples." << "\n";
//
//	constexpr const size_t allocatorSize =
//		(sizeof(size32Bytes) * sample_32_bytes +
//			sizeof(size256Bytes) * sample_256_bytes +
//			sizeof(size2593bytes) * sample_2593_bytes) * 2;
//
//	//Get some random values to test.
//	size_t randomValues[samples]{};
//	for (size_t i = 0; i < samples; i++)
//	{
//		randomValues[i] = static_cast<size_t>(BB::Utils::RandomUInt());
//	}
//
//	BB::POW_FreeListAllocator_t t_POW_FreeList(allocatorSize);
//
//	size32Bytes* size32Array = BB::BBallocArray<size32Bytes>(t_POW_FreeList, sample_32_bytes);
//	size256Bytes* size256Array = BB::BBallocArray<size256Bytes>(t_POW_FreeList, sample_256_bytes);
//	size2593bytes* size2593Array = BB::BBallocArray<size2593bytes>(t_POW_FreeList, sample_2593_bytes);
//
//	//Checking the arrays
//	for (size_t i = 0; i < sample_32_bytes; i++)
//	{
//		size32Array[i].value = randomValues[i];
//	}
//	for (size_t i = 0; i < sample_256_bytes; i++)
//	{
//		size256Array[i].value = randomValues[sample_32_bytes + i];
//	}
//	for (size_t i = 0; i < sample_2593_bytes; i++)
//	{
//		size2593Array[i].value = randomValues[sample_32_bytes + sample_256_bytes + i];
//	}
//
//	//Checking the arrays
//	for (size_t i = 0; i < sample_32_bytes; i++)
//	{
//		ASSERT_EQ(size32Array[i].value, randomValues[i]) << "32 bytes, Value is different in the freelist allocator.";
//	}
//	BB::BBFreeArray(t_POW_FreeList, size32Array);
//	for (size_t i = 0; i < sample_256_bytes; i++)
//	{
//		ASSERT_EQ(size256Array[i].value, randomValues[sample_32_bytes + i]) << "256 bytes, Value is different in the freelist allocator.";
//	}
//	BB::BBFreeArray(t_POW_FreeList, size256Array);
//	for (size_t i = 0; i < sample_2593_bytes; i++)
//	{
//		ASSERT_EQ(size2593Array[i].value, randomValues[sample_32_bytes + sample_256_bytes + i]) << "2593 bytes, Value is different in the freelist allocator.";
//	}
//	BB::BBFreeArray(t_POW_FreeList, size2593Array);
//	//Clear is not suppoted by freelist, commented just to show this is not a mistake.
//	//t_FreelistAllocator.Clear();
//}

#pragma endregion

#pragma region POOL_ALLOCATOR
TEST(MemoryAllocators, POOL_SINGLE_ALLOCATIONS)
{
	//std::cout << "Pool allocator with 1000 2593 bytes samples." << "\n";

	////Get some random values to test
	//size_t randomValues[sample_2593_bytes]{};
	//for (size_t i = 0; i < sample_2593_bytes; i++)
	//{
	//	randomValues[i] = static_cast<size_t>(BB::Utils::RandomUInt());
	//}

	//BB::PoolAllocator_t t_PoolAllocator(sizeof(size2593bytes), sample_2593_bytes, __alignof(size2593bytes));

	//for (size_t i = 0; i < sample_2593_bytes; i++)
	//{
	//	size2593bytes* sample = BB::BBalloc<size2593bytes>(t_PoolAllocator);
	//	sample->value = randomValues[i];
	//}

	////Test is depricated because of Boundrychecking.
	//Test all the values inside the allocations.
	//size2593bytes* t_AllocData = reinterpret_cast<size2593bytes*>(t_PoolAllocator.begin());
	//for (size_t i = 0; i < sample_2593_bytes; i++)
	//{
	//	ASSERT_EQ(t_AllocData[i].value, randomValues[i]) << "2593 bytes, Value is different in the Pool allocator.";
	//}

	//t_PoolAllocator.Clear();
}

TEST(MemoryAllocators, POOL_SINGLE_ALLOCATIONS_RESIZE)
{
	//std::cout << "Pool allocator with 1000 2593 bytes samples, but an allocator size for only 100 elements to test resizing." << "\n";

	////Get some random values to test
	//size_t randomValues[sample_2593_bytes]{};
	//for (size_t i = 0; i < sample_2593_bytes; i++)
	//{
	//	randomValues[i] = static_cast<size_t>(Utils::RandomUInt());
	//}

	//BB::unsafePoolAllocator_t t_PoolAllocator(sizeof(size2593bytes), sample_2593_bytes / 10, __alignof(size2593bytes));

	//for (size_t i = 0; i < sample_2593_bytes; i++)
	//{
	//	size2593bytes* sample = BB::AllocNew<size2593bytes>(t_PoolAllocator);
	//	sample->value = randomValues[i];
	//}

	////Test all the values inside the allocations.
	//size2593bytes* t_AllocData = reinterpret_cast<size2593bytes*>(t_PoolAllocator.begin());
	//for (size_t i = 0; i < sample_2593_bytes; i++)
	//{
	//	ASSERT_EQ(t_AllocData[i].value, randomValues[i]) << "2593 bytes, Value is different in the Pool allocator.";

	//}

	//t_PoolAllocator.Clear();
}

#pragma endregion