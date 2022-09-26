#include "Utils.h"

#include <immintrin.h>

namespace BB
{
	void BB::Memory::MemCpy(void* __restrict  a_Destination, const void* __restrict  a_Source, size_t a_Size)
	{
		//Get the registry size for most optimal memcpy.
		size_t* __restrict  t_Dest = reinterpret_cast<size_t*>(a_Destination);
		const size_t* __restrict  t_Src = reinterpret_cast<const size_t*>(a_Source);

		while (a_Size >= sizeof(size_t))
		{
			*t_Dest++ = *t_Src++;
			a_Size -= sizeof(size_t);
		}

		char* __restrict t_DestChar = reinterpret_cast<char*>(t_Dest);
		const char* __restrict t_SrcChar = reinterpret_cast<const char*>(t_Src);

		//Again but then go by byte.
		while (a_Size--)
		{
			*t_DestChar++ = *t_SrcChar++;
		}
	}

	void BB::Memory::MemCpySIMD128(void* __restrict  a_Destination, const void* __restrict  a_Source, size_t a_Size)
	{
		//Get the registry size for most optimal memcpy.
		__m128i* __restrict  t_Dest = reinterpret_cast<__m128i*>(a_Destination);
		const __m128i* __restrict  t_Src = reinterpret_cast<const __m128i*>(a_Source);

		while(a_Size >= sizeof(__m128i))
		{
			*t_Dest++ = _mm_loadu_epi64(t_Src++);
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

		while (a_Size >= sizeof(__m256i))
		{
			*t_Dest++ = _mm256_loadu_epi64(t_Src++);
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

	void BB::Memory::MemSet(void* __restrict  a_Destination, const int32_t a_Value, size_t a_Size)
	{
		//Get the registry size for most optimal memcpy.
		int32_t* __restrict  t_Dest = reinterpret_cast<int32_t*>(a_Destination);

		while (a_Size >= sizeof(int32_t))
		{
			*t_Dest++ = a_Value;
			a_Size -= sizeof(int32_t);
		}

		char* __restrict t_DestChar = reinterpret_cast<char*>(t_Dest);

		//Again but then go by byte.
		for (size_t i = 0; i < a_Size; i++)
		{
			*t_DestChar++ = reinterpret_cast<char*>(a_Value)[i];
		}
	}

	void BB::Memory::MemSetSIMD128(void* __restrict  a_Destination, const int32_t a_Value, size_t a_Size)
	{
		//Get the registry size for most optimal memcpy.
		__m128i* __restrict  t_Dest = reinterpret_cast<__m128i*>(a_Destination);

		while (a_Size >= sizeof(__m128i))
		{
			*t_Dest++ = _mm_set1_epi32(a_Value);
			a_Size -= sizeof(__m128i);
		}

		int32_t* __restrict  t_intDest = reinterpret_cast<int32_t*>(t_Dest);

		while (a_Size >= sizeof(int32_t))
		{
			*t_intDest++ = a_Value;
			a_Size -= sizeof(int32_t);
		}

		char* __restrict t_DestChar = reinterpret_cast<char*>(t_intDest);

		//Again but then go by byte.
		for (size_t i = 0; i < a_Size; i++)
		{
			*t_DestChar++ = reinterpret_cast<char*>(a_Value)[i];
		}
	}

	void BB::Memory::MemSetSIMD256(void* __restrict  a_Destination, const int32_t a_Value, size_t a_Size)
	{
		//Get the registry size for most optimal memcpy.
		__m256i* __restrict  t_Dest = reinterpret_cast<__m256i*>(a_Destination);

		while (a_Size > sizeof(__m256i))
		{
			*t_Dest++ = _mm256_set1_epi32(a_Value);
			a_Size -= sizeof(__m256i);
		}

		int32_t* __restrict  t_intDest = reinterpret_cast<int32_t*>(t_Dest);

		while (a_Size >= sizeof(int32_t))
		{
			*t_intDest++ = a_Value;
			a_Size -= sizeof(int32_t);
		}

		char* __restrict t_DestChar = reinterpret_cast<char*>(t_intDest);

		//Again but then go by byte.
		for (size_t i = 0; i < a_Size; i++)
		{
			*t_DestChar++ = reinterpret_cast<char*>(a_Value)[i];
		}
	}

	void BB::Memory::MemCmp(const void* __restrict  a_Left, const void* __restrict  a_Right, size_t a_Size)
	{

	}

	void BB::Memory::MemCmpSIMD128(const void* __restrict  a_Left, const void* __restrict  a_Right, size_t a_Size)
	{

	}

	void BB::Memory::MemCmpSIMD256(const void* __restrict  a_Left, const void* __restrict  a_Right, size_t a_Size)
	{
		//Get the registry size for most optimal memcpy.
		const __m256i* __restrict  t_Dest = reinterpret_cast<const __m256i*>(a_Left);



	}

}