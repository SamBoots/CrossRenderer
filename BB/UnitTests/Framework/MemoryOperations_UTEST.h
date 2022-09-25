#pragma once
#include "../TestValues.h"
#include "Allocators_UTEST.h"
#include <chrono>

constexpr const size_t kbSize = 1024;
constexpr const size_t mbSize = kbSize * 1024;
constexpr const size_t gbSize = mbSize * 1024;

static void FillBuffer(uint8_t* a_Value, size_t a_Size)
{
	const size_t t_Sequences = a_Size / 8;
	a_Value[0] = 0;
	a_Value[1] = 1;

	for (size_t i = 2; i < t_Sequences; i++)
	{
		a_Value[i] = a_Value[i - 1] + a_Value[i - 2];
	}
}


TEST(MemoryOperation_Speed_Comparison, Memcpy_Aligned)
{
	typedef std::chrono::duration<float, std::milli> ms;
	constexpr const float MILLITIMEDIVIDE = 1 / 1000.f;

	constexpr size_t SmallCopySize = 256;
	constexpr size_t MediumCopySize = mbSize;
	constexpr size_t BigCopySize = gbSize / 2;

	BB::FixedLinearAllocator_t t_FixedAllocator(SmallCopySize +
		MediumCopySize +
		BigCopySize * 2);

	uint8_t* smallBuffer = BB::BBnewArr<uint8_t>(t_FixedAllocator, SmallCopySize);
	FillBuffer(smallBuffer, SmallCopySize);
	uint8_t* mediumBuffer = BB::BBnewArr<uint8_t>(t_FixedAllocator, MediumCopySize);
	FillBuffer(mediumBuffer, MediumCopySize);
	uint8_t* bigBuffer = BB::BBnewArr<uint8_t>(t_FixedAllocator, BigCopySize);
	FillBuffer(bigBuffer, BigCopySize);

	uint8_t* copyBuffer = BB::BBnewArr<uint8_t>(t_FixedAllocator, BigCopySize);
	FillBuffer(copyBuffer, BigCopySize);



	{
		auto t_Timer = std::chrono::high_resolution_clock::now();

		BB::Memory::MemCpy(copyBuffer, smallBuffer, SmallCopySize);

		auto t_Speed = std::chrono::duration_cast<ms>(std::chrono::high_resolution_clock::now() - t_Timer).count() * MILLITIMEDIVIDE;
		std::cout << "BB 256 bytes Memcpy Speed in MS:" << t_Speed << "\n";
	}
	{
		auto t_Timer = std::chrono::high_resolution_clock::now();

		memcpy(copyBuffer, smallBuffer, SmallCopySize);

		auto t_Speed = std::chrono::duration_cast<ms>(std::chrono::high_resolution_clock::now() - t_Timer).count() * MILLITIMEDIVIDE;
		std::cout << "STL 256 bytes Memcpy Speed in MS:" << t_Speed << "\n";
	}

	{
		auto t_Timer = std::chrono::high_resolution_clock::now();

		BB::Memory::MemCpy(copyBuffer, mediumBuffer, MediumCopySize);

		auto t_Speed = std::chrono::duration_cast<ms>(std::chrono::high_resolution_clock::now() - t_Timer).count() * MILLITIMEDIVIDE;
		std::cout << "BB 1MB Memcpy Speed in MS:" << t_Speed << "\n";
	}
	{
		auto t_Timer = std::chrono::high_resolution_clock::now();

		memcpy(copyBuffer, mediumBuffer, MediumCopySize);

		auto t_Speed = std::chrono::duration_cast<ms>(std::chrono::high_resolution_clock::now() - t_Timer).count() * MILLITIMEDIVIDE;
		std::cout << "STL 1MB Memcpy Speed in MS:" << t_Speed << "\n";
	}

	{
		auto t_Timer = std::chrono::high_resolution_clock::now();

		BB::Memory::MemCpy(copyBuffer, bigBuffer, BigCopySize);

		auto t_Speed = std::chrono::duration_cast<ms>(std::chrono::high_resolution_clock::now() - t_Timer).count() * MILLITIMEDIVIDE;
		std::cout << "BB 512MB Memcpy Speed in MS:" << t_Speed << "\n";
	}
	{
		auto t_Timer = std::chrono::high_resolution_clock::now();

		memcpy(copyBuffer, bigBuffer, BigCopySize);

		auto t_Speed = std::chrono::duration_cast<ms>(std::chrono::high_resolution_clock::now() - t_Timer).count() * MILLITIMEDIVIDE;
		std::cout << "STL 512MB Memcpy Speed in MS:" << t_Speed << "\n";
	}

	t_FixedAllocator.Clear();
}