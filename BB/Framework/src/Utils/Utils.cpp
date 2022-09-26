#include "Utils.h"

#include <immintrin.h>

namespace BB
{
	void BB::Memory::MemCpy(void* __restrict  a_Destination, const void* __restrict  a_Source, size_t a_Size)
	{
		//Get the registry size for most optimal memcpy.
		uint8_t* __restrict  t_Dest = reinterpret_cast<uint8_t*>(a_Destination);
		const uint8_t* __restrict  t_Src = reinterpret_cast<const uint8_t*>(a_Source);

		//How many can we copy of 8 byte sized chunks?
		size_t t_Loopsize = (a_Size / sizeof(size_t));
		for (size_t i = 0; i < t_Loopsize; i++)
		{
			*reinterpret_cast<size_t*>(t_Dest) =
				*reinterpret_cast<const size_t*>(t_Src);

			t_Dest += sizeof(size_t);
			t_Src += sizeof(size_t);
		}

		//Again but then go by byte.
		t_Loopsize = (a_Size % sizeof(size_t));
		for (size_t i = a_Size - t_Loopsize; i < a_Size; i++)
		{
			*t_Dest = *t_Src;
			++t_Dest;
			++t_Src;
		}
	}

	void BB::Memory::MemCpySIMD128(void* __restrict  a_Destination, const void* __restrict  a_Source, size_t a_Size)
	{
		//Get the registry size for most optimal memcpy.
		__m128i* __restrict  t_Dest = reinterpret_cast<__m128i*>(a_Destination);
		const __m128i* __restrict  t_Src = reinterpret_cast<const __m128i*>(a_Source);

		while(a_Size > sizeof(__m128i))
		{
			*t_Dest++ = _mm_loadu_epi64(
				t_Src++);
			a_Size -= sizeof(__m128i);
		}

		char* __restrict t_DestChar = reinterpret_cast<char*>(t_Dest);
		const char* __restrict t_SrcChar = reinterpret_cast<const char*>(t_Src);

		//Again but then go by byte.
		while(a_Size--)
		{
			*t_DestChar++ = *t_SrcChar++;
		}
	}

	void BB::Memory::MemCpySIMD256(void* __restrict  a_Destination, const void* __restrict  a_Source, size_t a_Size)
	{
		//Get the registry size for most optimal memcpy.
		__m256i* __restrict  t_Dest = reinterpret_cast<__m256i*>(a_Destination);
		const __m256i* __restrict  t_Src = reinterpret_cast<const __m256i*>(a_Source);

		while (a_Size > sizeof(__m256i) * 4)
		{
			t_Dest[0] = _mm256_loadu_epi64(
				&t_Src[0]);
			t_Dest[1] = _mm256_loadu_epi64(
				&t_Src[1]);
			t_Dest[2] = _mm256_loadu_epi64(
				&t_Src[2]);
			t_Dest[3] = _mm256_loadu_epi64(
				&t_Src[3]);

			t_Dest += 4;
			t_Src += 4;
			a_Size -= sizeof(__m256i) * 4;
		}

		while (a_Size > sizeof(__m256i))
		{
			*t_Dest++ = _mm256_loadu_epi64(
				t_Src++);
			a_Size -= sizeof(__m256i);
		}

		char* __restrict t_DestChar = reinterpret_cast<char*>(t_Dest);
		const char* __restrict t_SrcChar = reinterpret_cast<const char*>(t_Src);

		//Again but then go by byte.
		while (a_Size--)
		{
			*t_DestChar++ = *t_SrcChar++;
		}
	}
}