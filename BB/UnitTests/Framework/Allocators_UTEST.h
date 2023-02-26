#pragma once
#include "../TestValues.h"
#include "BBMemory.h"
#include "Allocators/TemporaryAllocator.h"
#include "Allocators/RingAllocator.h"

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
		size32Bytes* sample = BBnew(t_LinearAllocator, size32Bytes);
		sample->value = randomValues[i];
	}
	for (size_t i = 0; i < sample_256_bytes; i++)
	{
		size256Bytes* sample = BBnew(t_LinearAllocator, size256Bytes);
		sample->value = randomValues[sample_32_bytes + i];
	}
	for (size_t i = 0; i < sample_2593_bytes; i++)
	{
		size2593bytes* sample = BBnew(t_LinearAllocator, size2593bytes);
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

	size32Bytes* size32Array = BBnewArr(t_LinearAllocator, sample_32_bytes, size32Bytes);
	size256Bytes* size256Array = BBnewArr(t_LinearAllocator, sample_256_bytes, size256Bytes);
	size2593bytes* size2593Array = BBnewArr(t_LinearAllocator, sample_2593_bytes, size2593bytes);
	
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

	BB::FreelistAllocator_t t_FreelistAllocator(allocatorSize);

	{
		//This address should always be used since it's a free block.
		void* repeatAddress32 = BBnew(t_FreelistAllocator, size32Bytes);
		BB::BBfree(t_FreelistAllocator, repeatAddress32);

		for (size_t i = 0; i < sample_32_bytes; i++)
		{
			size32Bytes* sample = BBnew(t_FreelistAllocator, size32Bytes);
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
		void* repeatAddress256 = BBnew(t_FreelistAllocator, size256Bytes);
		BB::BBfree(t_FreelistAllocator, repeatAddress256);

		for (size_t i = 0; i < sample_256_bytes; i++)
		{
			size256Bytes* sample = BBnew(t_FreelistAllocator, size256Bytes);
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
		void* repeatAddress2593 = BBnew(t_FreelistAllocator, size2593bytes);
		BB::BBfree(t_FreelistAllocator, repeatAddress2593);

		for (size_t i = 0; i < sample_2593_bytes; i++)
		{
			size2593bytes* sample = BBnew(t_FreelistAllocator, size2593bytes);
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

	BB::FreelistAllocator_t t_FreeList(allocatorSize);

	size32Bytes* size32Array = BBnewArr(t_FreeList, sample_32_bytes, size32Bytes);
	size256Bytes* size256Array = BBnewArr(t_FreeList, sample_256_bytes, size256Bytes);
	size2593bytes* size2593Array = BBnewArr(t_FreeList, sample_2593_bytes, size2593bytes);

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

	BB::POW_FreelistAllocator_t t_POW_FreelistAllocator(allocatorSize);

	{
		////This address should always be used since it's a free block.
		void* repeatAddress32 = BBnew(t_POW_FreelistAllocator, size32Bytes);
		BB::BBfree(t_POW_FreelistAllocator, repeatAddress32);

		for (size_t i = 0; i < sample_32_bytes; i++)
		{
			size32Bytes* sample = BBnew(t_POW_FreelistAllocator, size32Bytes);
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
		void* repeatAddress256 = BBnew(t_POW_FreelistAllocator, size256Bytes);
		BB::BBfree(t_POW_FreelistAllocator, repeatAddress256);

		for (size_t i = 0; i < sample_256_bytes; i++)
		{
			size256Bytes* sample = BBnew(t_POW_FreelistAllocator, size256Bytes);
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
		void* repeatAddress2593 = BBnew(t_POW_FreelistAllocator, size2593bytes);
		BB::BBfree(t_POW_FreelistAllocator, repeatAddress2593);

		for (size_t i = 0; i < sample_2593_bytes; i++)
		{
			size2593bytes* sample = BBnew(t_POW_FreelistAllocator, size2593bytes);
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

#pragma region TEMPORARY_ALLOCATOR
TEST(MemoryAllocators, TEMPORARY_ALLOCATOR)
{
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

	BB::FreelistAllocator_t t_Backing(allocatorSize * 2);
	BB::TemporaryAllocator t_TempAlloc(t_Backing);

	for (size_t i = 0; i < sample_32_bytes; i++)
	{
		size32Bytes* sample = BBnew(t_TempAlloc, size32Bytes);
		sample->value = randomValues[i];
	}
	for (size_t i = 0; i < sample_256_bytes; i++)
	{
		size256Bytes* sample = BBnew(t_TempAlloc, size256Bytes);
		sample->value = randomValues[sample_32_bytes + i];
	}
	for (size_t i = 0; i < sample_2593_bytes; i++)
	{
		size2593bytes* sample = BBnew(t_TempAlloc, size2593bytes);
		sample->value = randomValues[sample_32_bytes + sample_256_bytes + i];
	}

	t_TempAlloc.Clear();

	for (size_t i = 0; i < sample_32_bytes; i++)
	{
		size32Bytes* sample = BBnew(t_TempAlloc, size32Bytes);
		sample->value = randomValues[i];
	}
	for (size_t i = 0; i < sample_256_bytes; i++)
	{
		size256Bytes* sample = BBnew(t_TempAlloc, size256Bytes);
		sample->value = randomValues[sample_32_bytes + i];
	}
	for (size_t i = 0; i < sample_2593_bytes; i++)
	{
		size2593bytes* sample = BBnew(t_TempAlloc, size2593bytes);
		sample->value = randomValues[sample_32_bytes + sample_256_bytes + i];
	}
}

#pragma endregion //TEMPORARY_ALLOCATOR


#pragma region RING_ALLOCATOR
TEST(MemoryAllocators, RING_ALLOCATOR)
{
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

	BB::FreelistAllocator_t t_Backing(allocatorSize * 2);
	//Getting half of the allocator size will show that the ring allocator going back to start will work.
	BB::RingAllocator t_TempAlloc(t_Backing, allocatorSize / 2);

	for (size_t i = 0; i < sample_32_bytes; i++)
	{
		size32Bytes* sample = BBnew(t_TempAlloc, size32Bytes);
		sample->value = randomValues[i];
	}
	for (size_t i = 0; i < sample_256_bytes; i++)
	{
		size256Bytes* sample = BBnew(t_TempAlloc, size256Bytes);
		sample->value = randomValues[sample_32_bytes + i];
	}
	for (size_t i = 0; i < sample_2593_bytes; i++)
	{
		size2593bytes* sample = BBnew(t_TempAlloc, size2593bytes);
		sample->value = randomValues[sample_32_bytes + sample_256_bytes + i];
	}

	//This will cause the ringallocator to go back to start again.
	for (size_t i = 0; i < sample_2593_bytes; i++)
	{
		size2593bytes* sample = BBnew(t_TempAlloc, size2593bytes);
		sample->value = randomValues[sample_32_bytes + sample_256_bytes + i];
	}
}

#pragma endregion //RING_ALLOCATOR