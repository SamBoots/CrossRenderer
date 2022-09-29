#pragma once
#include "../TestValues.h"
#include "Storage/Hashmap.h"

TEST(Hashmap_Datastructure, UM_Hashmap_Insert_Copy_Assignment)
{
	constexpr const size_t samples = 256;

	//32 MB alloactor.
	const size_t allocatorSize = BB::mbSize * 32;
	BB::FreelistAllocator_t t_Allocator(allocatorSize);

	BB::UM_HashMap<size_t, size2593bytesObj> t_Map(t_Allocator);

	{
		size2593bytesObj t_Value{};
		t_Value.value = 500;
		size_t t_Key = 124;
		t_Map.insert(t_Key, t_Value);

		ASSERT_NE(t_Map.find(t_Key), nullptr) << "Cannot find the element while it was added!";
		ASSERT_EQ(t_Map.find(t_Key)->value, t_Value.value) << "Wrong element was likely grabbed.";

		t_Map.erase(t_Key);
		ASSERT_EQ(t_Map.find(t_Key), nullptr) << "Element was found while it should've been deleted.";
	}

	size_t t_RandomKeys[samples]{};
	for (size_t i = 0; i < samples; i++)
	{
		t_RandomKeys[i] = static_cast<size_t>(BB::Random::Random());
	}

	for (size_t i = 0; i < samples; i++)
	{
		size2593bytesObj t_Value{};
		t_Value.value = 50 * i;
		size_t t_Key = t_RandomKeys[i];
		t_Map.insert(t_Key, t_Value);

		ASSERT_NE(t_Map.find(t_Key), nullptr) << "Cannot find the element while it was added!";
		ASSERT_EQ(t_Map.find(t_Key)->value, t_Value.value) << "Wrong element was likely grabbed.";
	}

	//Copy Constructor
	BB::UM_HashMap<size_t, size2593bytesObj> t_CopyMap(t_Map);

	for (uint32_t i = 0; i < samples; i++)
	{
		size_t t_Key = t_RandomKeys[i];

		ASSERT_EQ(t_CopyMap.find(t_Key)->value, t_Map.find(t_Key)->value) << "Wrong element was grabbed from the copy of the map.";
	}

	BB::UM_HashMap<size_t, size2593bytesObj> t_CopyOperatorMap(t_Allocator);
	t_CopyOperatorMap = t_CopyMap;

	for (uint32_t i = 0; i < samples; i++)
	{
		size_t t_Key = t_RandomKeys[i];

		ASSERT_EQ(t_CopyOperatorMap.find(t_Key)->value, t_CopyMap.find(t_Key)->value) << "Wrong element was grabbed from the copy of the map.";
	}

	//Assignment Constructor
	BB::UM_HashMap<size_t, size2593bytesObj> t_AssignmentMap(std::move(t_CopyOperatorMap));
	ASSERT_EQ(t_CopyOperatorMap.size(), 0);

	for (size_t i = 0; i < samples; i++)
	{
		size_t t_Key = t_RandomKeys[i];
		size2593bytesObj t_Value{};
		t_Value.value = 50 * i;

		ASSERT_EQ(t_AssignmentMap.find(t_Key)->value, t_Value.value) << "Wrong element was grabbed from the copy of the map.";
	}

	//Assignment Operator
	BB::UM_HashMap<size_t, size2593bytesObj> t_AssignmentOperatorMap(t_Allocator);
	t_AssignmentOperatorMap = std::move(t_AssignmentMap);
	ASSERT_EQ(t_AssignmentMap.size(), 0);

	for (size_t i = 0; i < samples; i++)
	{
		size_t t_Key = t_RandomKeys[i];
		size2593bytesObj t_Value{};
		t_Value.value = 50 * i;

		ASSERT_EQ(t_AssignmentOperatorMap.find(t_Key)->value, t_Value.value) << "Wrong element was grabbed from the copy of the map.";
	}
}

TEST(Hashmap_Datastructure, UM_Hashmap_Range_Based_Loop)
{
	constexpr const uint32_t samples = 4096;

	//32 MB alloactor.
	const size_t allocatorSize = BB::mbSize * 32;
	BB::FreelistAllocator_t t_Allocator(allocatorSize);

	BB::UM_HashMap<size_t, size2593bytesObj> t_Map(t_Allocator);
	t_Map.reserve(samples);

	size_t t_RandomKeys[samples]{};
	for (size_t i = 0; i < samples; i++)
	{
		t_RandomKeys[i] = (i + 1) * 2;
	}

	//Only test a quarter of the samples for just inserting and removing.
	for (size_t i = 0; i < samples; i++)
	{
		size2593bytesObj t_Value{};
		t_Value.value = 5 + i * 2;
		size_t t_Key = t_RandomKeys[i];
		t_Map.insert(t_Key, t_Value);

		ASSERT_NE(t_Map.find(t_Key), nullptr) << "Cannot find the element while it was added!";
		ASSERT_EQ(t_Map.find(t_Key)->value, t_Value.value) << "Wrong element was likely grabbed.";
	}

	size_t t_IteratorCount = 0;
	size_t t_PreviousKey = 0xDEADBEEF;
	for (auto t_It = t_Map.begin(); t_It < t_Map.end(); t_It++)
	{
		++t_IteratorCount;
		size_t t_Key = t_It->key;
		ASSERT_EQ(t_Map.find(t_Key)->value, t_It->value.value) << "Iterator found an pair that the hashmap couldn't find.";
		ASSERT_NE(t_Key, t_PreviousKey) << "You read the same key twice.";
		t_PreviousKey = t_Key;
	}
	ASSERT_EQ(t_IteratorCount, samples) << "Did not iterate over all the values.";

	//Not supporting range based loops.
	//for (auto& t_It : t_Map)
	//{
	//	ASSERT_EQ(t_Map.find(*t_It.key)->value, t_It.value->value) << "Iterator found an pair that the hashmap couldn't find.";
	//}
}

TEST(Hashmap_Datastructure, OL_Hashmap_Insert_Copy_Assignment)
{
	constexpr const uint32_t samples = 4096;

	//32 MB alloactor.
	const size_t allocatorSize = BB::mbSize * 32;
	BB::FreelistAllocator_t t_Allocator(allocatorSize);

	BB::OL_HashMap<size_t, size2593bytesObj> t_Map(t_Allocator);
	t_Map.reserve(samples);
	{
		size2593bytesObj t_Value{};
		t_Value.value = 500;
		size_t t_Key = 124;
		t_Map.insert(t_Key, t_Value);

		ASSERT_NE(t_Map.find(t_Key), nullptr) << "Cannot find the element while it was added!";
		ASSERT_EQ(t_Map.find(t_Key)->value, t_Value.value) << "Wrong element was likely grabbed.";

		t_Map.erase(t_Key);
		ASSERT_EQ(t_Map.find(t_Key), nullptr) << "Element was found while it should've been deleted.";
	}

	size_t t_RandomKeys[samples]{};
	for (size_t i = 0; i < samples; i++)
	{
		t_RandomKeys[i] = (i + 1) * 2;
	}

	//Only test a quarter of the samples for just inserting and removing.
	for (size_t i = 0; i < samples / 4; i++)
	{
		size2593bytesObj t_Value{};
		t_Value.value = 500;
		size_t t_Key = t_RandomKeys[i];
		t_Map.insert(t_Key, t_Value);

		ASSERT_NE(t_Map.find(t_Key), nullptr) << "Cannot find the element while it was added!";
		ASSERT_EQ(t_Map.find(t_Key)->value, t_Value.value) << "Wrong element was likely grabbed.";

		t_Map.erase(t_Key);
		ASSERT_EQ(t_Map.find(t_Key), nullptr) << "Element was found while it should've been deleted.";
	}

	//FIll the map to double it's size and test if a resize works.
	for (size_t i = 0; i < samples; i++)
	{
		size2593bytesObj t_Value{};
		t_Value.value = t_RandomKeys[i] + 2;
		size_t t_Key = t_RandomKeys[i];
		t_Map.insert(t_Key, t_Value);
	}
	//Now check it
	for (size_t i = 0; i < samples; i++)
	{
		size_t t_Key = t_RandomKeys[i];

		EXPECT_NE(t_Map.find(t_Key), nullptr) << " Cannot find the element while it was added!";
		if (t_Map.find(t_Key) != nullptr)
			EXPECT_EQ(t_Map.find(t_Key)->value, t_Key + 2) << "element: " << i << " Wrong element was likely grabbed.";
	}

	//Copy Constructor
	BB::OL_HashMap<size_t, size2593bytesObj> t_CopyMap(t_Map);

	for (uint32_t i = 0; i < samples; i++)
	{
		size_t t_Key = t_RandomKeys[i];

		ASSERT_EQ(t_CopyMap.find(t_Key)->value, t_Map.find(t_Key)->value) << "Wrong element was grabbed from the copy of the map.";
	}

	BB::OL_HashMap<size_t, size2593bytesObj> t_CopyOperatorMap(t_Allocator);
	t_CopyOperatorMap = t_CopyMap;

	for (uint32_t i = 0; i < samples; i++)
	{
		size_t t_Key = t_RandomKeys[i];

		ASSERT_EQ(t_CopyOperatorMap.find(t_Key)->value, t_CopyMap.find(t_Key)->value) << "Wrong element was grabbed from the copy of the map.";
	}

	//Assignment Constructor
	BB::OL_HashMap<size_t, size2593bytesObj> t_AssignmentMap(std::move(t_CopyOperatorMap));
	ASSERT_EQ(t_CopyOperatorMap.size(), 0);

	for (size_t i = 0; i < samples; i++)
	{
		size_t t_Key = t_RandomKeys[i];

		ASSERT_EQ(t_AssignmentMap.find(t_Key)->value, t_Map.find(t_Key)->value) << "Wrong element was grabbed from the copy of the map.";
	}

	//Assignment Operator
	BB::OL_HashMap<size_t, size2593bytesObj> t_AssignmentOperatorMap(t_Allocator);
	t_AssignmentOperatorMap = std::move(t_AssignmentMap);
	ASSERT_EQ(t_AssignmentMap.size(), 0);

	for (size_t i = 0; i < samples; i++)
	{
		size_t t_Key = t_RandomKeys[i];

		ASSERT_EQ(t_AssignmentOperatorMap.find(t_Key)->value, t_Map.find(t_Key)->value) << "Wrong element was grabbed from the copy of the map.";
	}
}

TEST(Hashmap_Datastructure, OL_Hashmap_Range_Based_Loop)
{
	constexpr const uint32_t samples = 4096;

	//32 MB alloactor.
	const size_t allocatorSize = BB::mbSize * 32;
	BB::FreelistAllocator_t t_Allocator(allocatorSize);

	BB::OL_HashMap<size_t, size2593bytesObj> t_Map(t_Allocator);
	t_Map.reserve(samples);

	size_t t_RandomKeys[samples]{};
	for (size_t i = 0; i < samples; i++)
	{
		t_RandomKeys[i] = (i + 1) * 2;
	}

	//Only test a quarter of the samples for just inserting and removing.
	for (size_t i = 0; i < samples; i++)
	{
		size2593bytesObj t_Value{};
		t_Value.value = 5 + i * 2;
		size_t t_Key = t_RandomKeys[i];
		t_Map.insert(t_Key, t_Value);

		ASSERT_NE(t_Map.find(t_Key), nullptr) << "Cannot find the element while it was added!";
		ASSERT_EQ(t_Map.find(t_Key)->value, t_Value.value) << "Wrong element was likely grabbed.";
	}

	size_t t_IteratorCount = 0;
	size_t t_PreviousKey = 0xDEADBEEF;
	for (auto t_It = t_Map.begin(); t_It < t_Map.end(); t_It++)
	{
		++t_IteratorCount;
		size_t t_Key = *t_It->key;
		ASSERT_EQ(t_Map.find(t_Key)->value, t_It->value->value) << "Iterator found an pair that the hashmap couldn't find.";
		ASSERT_NE(t_Key, t_PreviousKey) << "You read the same key twice.";
		t_PreviousKey = t_Key;
	}
	ASSERT_EQ(t_IteratorCount, samples) << "Did not iterate over all the values.";

	//Not supporting range based loops.
	//for (auto& t_It : t_Map)
	//{
	//	ASSERT_EQ(t_Map.find(*t_It.key)->value, t_It.value->value) << "Iterator found an pair that the hashmap couldn't find.";
	//}
}

#include <chrono>
#include <unordered_map>

TEST(Hashmap_Datastructure, Hashmap_Speedtest)
{
	//Some tools for the speed test.
	typedef std::chrono::duration<float, std::milli> ms;
	constexpr const float MILLITIMEDIVIDE = 1 / 1000.f;
#ifdef _64BIT
	constexpr const size_t samples = 8192;

	const size_t allocatorSize = BB::mbSize * 32;
#endif //_64BIT
#ifdef _32BIT
	constexpr const size_t samples = 2048;

	const size_t allocatorSize = BB::mbSize * 8;
#endif //_32BIT


	BB::FreelistAllocator_t t_Allocator(allocatorSize);

	//all the maps
	std::unordered_map<size_t, size2593bytesObj> t_UnorderedMap;
	BB::UM_HashMap<size_t, size2593bytesObj> t_UM_Map(t_Allocator);
	BB::OL_HashMap<size_t, size2593bytesObj> t_OL_Map(t_Allocator);

	t_UnorderedMap.reserve(samples);
	t_UM_Map.reserve(samples);
	t_OL_Map.reserve(samples);

	//The samples we will use as an example.
	size_t t_RandomKeys[samples]{};
	for (uint32_t i = 0; i < samples; i++)
	{
		t_RandomKeys[i] = static_cast<size_t>(BB::Random::Random());
	}

	std::cout << "Hashmap speed test comparison with" << "\n" << "std::unordered_map" << "\n" << "BB::UM_Hashmap" << "\n" << "BB::OL_Hashmap" << "\n" << "\n";
	std::cout << "The element sizes being added to the hashmap are 2593 bytes in size and have a constructor/deconstructor." << "\n";
	std::cout << "The amount of samples per hashmap: " << samples << "\n" << "\n";
	
	std::cout << "/-----------------------------------------/" << "\n" << "Insert Speed Test:" << "\n";
#pragma region Insert_Test
	{	
		auto t_Timer = std::chrono::high_resolution_clock::now();
		//Unordered Map speed.
		for (size_t i = 0; i < samples; i++)
		{
			size2593bytesObj t_Insert{};
			t_Insert.value = i;
			t_UnorderedMap.emplace(std::make_pair(t_RandomKeys[i], t_Insert));
		}
		auto t_Unordered_MapSpeed = std::chrono::duration_cast<ms>(std::chrono::high_resolution_clock::now() - t_Timer).count() * MILLITIMEDIVIDE;
		std::cout << "Unordered map speed with time in MS " << t_Unordered_MapSpeed << "\n";
	}

	{
		auto t_Timer = std::chrono::high_resolution_clock::now();
		//BB::UM speed.
		for (size_t i = 0; i < samples; i++)
		{
			size2593bytesObj t_Insert{};
			t_Insert.value = i;
			t_UM_Map.emplace(t_RandomKeys[i], t_Insert);
		}
		auto t_UMMapSpeed = std::chrono::duration_cast<ms>(std::chrono::high_resolution_clock::now() - t_Timer).count() * MILLITIMEDIVIDE;
		std::cout << "UM map speed with time in MS " << t_UMMapSpeed << "\n";
	}

	{
		
		auto t_Timer = std::chrono::high_resolution_clock::now();
		//BB::OL speed.
		for (size_t i = 0; i < samples; i++)
		{
			size2593bytesObj t_Insert{};
			t_Insert.value = i;
			t_OL_Map.emplace(t_RandomKeys[i], t_Insert.value);
		}
		auto t_OLMapSpeed = std::chrono::duration_cast<ms>(std::chrono::high_resolution_clock::now() - t_Timer).count() * MILLITIMEDIVIDE;
		std::cout << "OL map speed with time in MS " << t_OLMapSpeed << "\n";
	}

#pragma endregion
	std::cout << "/-----------------------------------------/" << "\n" << "Lookup Speed Test:" << "\n";
#pragma region Lookup_Test

	{
		auto t_Timer = std::chrono::high_resolution_clock::now();
		//Unordered Map speed.
		for (size_t i = 0; i < samples; i++)
		{
			EXPECT_EQ(t_UnorderedMap.find(t_RandomKeys[i])->second.value, i) << "std unordered Hashmap couldn't find key " << t_RandomKeys[i];
		}
		auto t_Unordered_MapSpeed = std::chrono::duration_cast<ms>(std::chrono::high_resolution_clock::now() - t_Timer).count() * MILLITIMEDIVIDE;
		std::cout << "Unordered map speed with time in MS " << t_Unordered_MapSpeed << "\n";
	}

	{
		auto t_Timer = std::chrono::high_resolution_clock::now();
		//BB::UM speed.
		for (size_t i = 0; i < samples; i++)
		{
			EXPECT_EQ(t_UM_Map.find(t_RandomKeys[i])->value, i) << "UM Hashmap couldn't find key " << t_RandomKeys[i];
		}
		auto t_UMMapSpeed = std::chrono::duration_cast<ms>(std::chrono::high_resolution_clock::now() - t_Timer).count() * MILLITIMEDIVIDE;
		std::cout << "UM map speed with time in MS " << t_UMMapSpeed << "\n";
	}

	{

		auto t_Timer = std::chrono::high_resolution_clock::now();
		//BB::OL speed.
		for (size_t i = 0; i < samples; i++)
		{
			EXPECT_EQ(t_OL_Map.find(t_RandomKeys[i])->value, i) << "OL Hashmap couldn't find key " << t_RandomKeys[i];
		}
		auto t_OLMapSpeed = std::chrono::duration_cast<ms>(std::chrono::high_resolution_clock::now() - t_Timer).count() * MILLITIMEDIVIDE;
		std::cout << "OL map speed with time in MS " << t_OLMapSpeed << "\n";
	}

#pragma endregion
	std::cout << "/-----------------------------------------/" << "\n" << "Lookup Empty Speed Test:" << "\n";
#pragma region Lookup_Empty_Test

	constexpr const size_t EMPTY_KEY = 414141414141;
	{
		auto t_Timer = std::chrono::high_resolution_clock::now();
		//Unordered Map speed.
		for (size_t i = 0; i < samples; i++)
		{
			EXPECT_EQ(t_UnorderedMap.find(EMPTY_KEY + i), t_UnorderedMap.end()) << "std unordered Hashmap found a key while it shouldn't exist." << t_RandomKeys[i];
		}
		auto t_Unordered_MapSpeed = std::chrono::duration_cast<ms>(std::chrono::high_resolution_clock::now() - t_Timer).count() * MILLITIMEDIVIDE;
		std::cout << "Unordered map speed with time in MS " << t_Unordered_MapSpeed << "\n";
	}

	{
		auto t_Timer = std::chrono::high_resolution_clock::now();
		//BB::UM speed.
		for (size_t i = 0; i < samples; i++)
		{
			EXPECT_EQ(t_UM_Map.find(EMPTY_KEY + i), nullptr) << "UM Hashmap found a key while it shouldn't exist." << t_RandomKeys[i];
		}
		auto t_UMMapSpeed = std::chrono::duration_cast<ms>(std::chrono::high_resolution_clock::now() - t_Timer).count() * MILLITIMEDIVIDE;
		std::cout << "UM map speed with time in MS " << t_UMMapSpeed << "\n";
	}

	{

		auto t_Timer = std::chrono::high_resolution_clock::now();
		//BB::OL speed.
		for (size_t i = 0; i < samples; i++)
		{
			EXPECT_EQ(t_OL_Map.find(EMPTY_KEY + i), nullptr) << "OL Hashmap found a key while it shouldn't exist." << t_RandomKeys[i];
		}
		auto t_OLMapSpeed = std::chrono::duration_cast<ms>(std::chrono::high_resolution_clock::now() - t_Timer).count() * MILLITIMEDIVIDE;
		std::cout << "OL map speed with time in MS " << t_OLMapSpeed << "\n";
	}

#pragma endregion
	std::cout << "/-----------------------------------------/" << "\n" << "Iterator Speed Test, it also sets all values to 0." << "\n";
#pragma region Iterator_Test

	{
		auto t_Timer = std::chrono::high_resolution_clock::now();
		//Unordered Map speed.
		size_t t_IteratorCount = 0;
		//unordered map works with != instead of < so we use this.
		for (auto t_It = t_UnorderedMap.begin(); t_It != t_UnorderedMap.end(); t_It++)
		{
			t_It->second.value = 100 * 5 * 2;
			t_It->second.value = 0;
			++t_IteratorCount;
		}
		EXPECT_EQ(t_IteratorCount, samples) << "unordered_map, Iterated too many times";
		auto t_Unordered_MapSpeed = std::chrono::duration_cast<ms>(std::chrono::high_resolution_clock::now() - t_Timer).count() * MILLITIMEDIVIDE;
		std::cout << "Unordered map speed with time in MS " << t_Unordered_MapSpeed << "\n";
	}

	{
		auto t_Timer = std::chrono::high_resolution_clock::now();
		//BB::UM speed.
		size_t t_IteratorCount = 0;
		for (auto t_It = t_UM_Map.begin(); t_It < t_UM_Map.end(); t_It++)
		{
			t_It->value.value = 100 * 5 * 2;
			t_It->value.value = 0;
			++t_IteratorCount;
		}
		EXPECT_EQ(t_IteratorCount, samples) << "UM map, Iterated too many times";
		auto t_UMMapSpeed = std::chrono::duration_cast<ms>(std::chrono::high_resolution_clock::now() - t_Timer).count() * MILLITIMEDIVIDE;
		std::cout << "UM map speed with time in MS " << t_UMMapSpeed << "\n";
	}

	{

		auto t_Timer = std::chrono::high_resolution_clock::now();
		//BB::OL speed.
		size_t t_IteratorCount = 0;
		for (auto t_It = t_OL_Map.begin(); t_It < t_OL_Map.end(); t_It++)
		{
			t_It->value->value = 100 * 5 * 2;
			t_It->value->value = 0;
			++t_IteratorCount;
		}
		EXPECT_EQ(t_IteratorCount, samples) << "OL map, Iterated too many times";
		auto t_OLMapSpeed = std::chrono::duration_cast<ms>(std::chrono::high_resolution_clock::now() - t_Timer).count() * MILLITIMEDIVIDE;
		std::cout << "OL map speed with time in MS " << t_OLMapSpeed << "\n";
	}

#pragma endregion
	std::cout << "/-----------------------------------------/" << "\n" << "Erase Speed Test:" << "\n";
#pragma region Erase_Test
	{
		auto t_Timer = std::chrono::high_resolution_clock::now();
		//Unordered Map speed.
		for (size_t i = 0; i < samples; i++)
		{
			t_UnorderedMap.erase(t_RandomKeys[i]);
		}
		auto t_Unordered_MapSpeed = std::chrono::duration_cast<ms>(std::chrono::high_resolution_clock::now() - t_Timer).count() * MILLITIMEDIVIDE;
		std::cout << "Unordered map speed with time in MS " << t_Unordered_MapSpeed << "\n";
	}

	{
		auto t_Timer = std::chrono::high_resolution_clock::now();
		//BB::UM speed.
		for (size_t i = 0; i < samples; i++)
		{
			t_UM_Map.erase(t_RandomKeys[i]);
		}
		auto t_UMMapSpeed = std::chrono::duration_cast<ms>(std::chrono::high_resolution_clock::now() - t_Timer).count() * MILLITIMEDIVIDE;
		std::cout << "UM map speed with time in MS " << t_UMMapSpeed << "\n";
	}

	{
		auto t_Timer = std::chrono::high_resolution_clock::now();
		//BB::OL speed.
		for (size_t i = 0; i < samples; i++)
		{
			t_OL_Map.erase(t_RandomKeys[i]);
		}
		auto t_OLMapSpeed = std::chrono::duration_cast<ms>(std::chrono::high_resolution_clock::now() - t_Timer).count() * MILLITIMEDIVIDE;
		std::cout << "OL map speed with time in MS " << t_OLMapSpeed << "\n";
	}
#pragma endregion
	std::cout << "/-----------------------------------------/" << "\n";
}