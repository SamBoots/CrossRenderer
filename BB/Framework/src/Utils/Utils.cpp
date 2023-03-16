#include "Utils.h"
#include "Math.inl"

#include <immintrin.h>


namespace BB
{
	void BB::Memory::MemCpy(void* __restrict  a_Destination, const void* __restrict  a_Source, size_t a_Size)
	{
		const size_t t_Diff = Pointer::AlignForwardAdjustment(a_Destination, sizeof(size_t));

		uint8_t* __restrict t_AlignDest = reinterpret_cast<uint8_t*>(a_Destination);
		const uint8_t* __restrict t_AlignSrc = reinterpret_cast<const uint8_t*>(a_Source);

		for (size_t i = 0; i < t_Diff; i++)
		{
			*t_AlignDest++ = *t_AlignSrc++;
		}

		a_Size -= t_Diff;

		//Get the registry size for most optimal memcpy.
		size_t* __restrict  t_Dest = reinterpret_cast<size_t*>(t_AlignDest);
		const size_t* __restrict  t_Src = reinterpret_cast<const size_t*>(t_AlignSrc);

		while (a_Size >= sizeof(size_t))
		{
			*t_Dest++ = *t_Src++;
			a_Size -= sizeof(size_t);
		}

		uint8_t* __restrict t_DestChar = reinterpret_cast<uint8_t*>(t_Dest);
		const uint8_t* __restrict t_SrcChar = reinterpret_cast<const uint8_t*>(t_Src);

		//Again but then go by byte.
		while (a_Size--)
		{
			*t_DestChar++ = *t_SrcChar++;
		}
	}

	void BB::Memory::MemCpySIMD128(void* __restrict  a_Destination, const void* __restrict a_Source, size_t a_Size)
	{
		const size_t t_Diff = Pointer::AlignForwardAdjustment(a_Destination, sizeof(__m128i));

		uint8_t* __restrict t_AlignDest = reinterpret_cast<uint8_t*>(a_Destination);
		const uint8_t* __restrict t_AlignSrc = reinterpret_cast<const uint8_t*>(a_Source);

		for (size_t i = 0; i < t_Diff; i++)
		{
			*t_AlignDest++ = *t_AlignSrc++;
		}

		a_Size -= t_Diff;

		//Get the registry size for most optimal memcpy.
		__m128i* __restrict  t_Dest = reinterpret_cast<__m128i*>(t_AlignDest);
		const __m128i* __restrict  t_Src = reinterpret_cast<const __m128i*>(t_AlignSrc);

		while(a_Size >= sizeof(__m128i))
		{
			*t_Dest++ = _mm_loadu_epi64(t_Src++);
			a_Size -= sizeof(__m128i);
		}

		uint8_t* __restrict t_DestChar = reinterpret_cast<uint8_t*>(t_Dest);
		const uint8_t* __restrict t_SrcChar = reinterpret_cast<const uint8_t*>(t_Src);

		//Again but then go by byte.
		while(a_Size--)
		{
			*t_DestChar++ = *t_SrcChar++;
		}
	}

	void BB::Memory::MemCpySIMD256(void* __restrict  a_Destination, const void* __restrict  a_Source, size_t a_Size)
	{
		const size_t t_Diff = Pointer::AlignForwardAdjustment(a_Destination, sizeof(__m256i));

		uint8_t* __restrict t_AlignDest = reinterpret_cast<uint8_t*>(a_Destination);
		const uint8_t* __restrict t_AlignSrc = reinterpret_cast<const uint8_t*>(a_Source);

		for (size_t i = 0; i < t_Diff; i++)
		{
			*t_AlignDest++ = *t_AlignSrc++;
		}

		a_Size -= t_Diff;

		//Get the registry size for most optimal memcpy.
		__m256i* __restrict  t_Dest = reinterpret_cast<__m256i*>(t_AlignDest);
		const __m256i* __restrict  t_Src = reinterpret_cast<const __m256i*>(t_AlignSrc);

		while (a_Size >= sizeof(__m256i))
		{
			*t_Dest++ = _mm256_loadu_epi64(t_Src++);
			a_Size -= sizeof(__m256i);
		}

		uint8_t* __restrict t_DestChar = reinterpret_cast<uint8_t*>(t_Dest);
		const uint8_t* __restrict t_SrcChar = reinterpret_cast<const uint8_t*>(t_Src);

		//Again but then go by byte.
		while (a_Size--)
		{
			*t_DestChar++ = *t_SrcChar++;
		}
	}

	void BB::Memory::MemSet(void* __restrict  a_Destination, const int32_t a_Value, size_t a_Size)
	{
		//Get the registry size for most optimal memcpy.
		size_t* __restrict  t_Dest = reinterpret_cast<size_t*>(a_Destination);

		while (a_Size >= sizeof(size_t))
		{
			*t_Dest++ = a_Value;
			a_Size -= sizeof(size_t);
		}

		uint8_t* __restrict t_DestChar = reinterpret_cast<uint8_t*>(t_Dest);

		//Again but then go by byte.
		for (size_t i = 0; i < a_Size; i++)
		{
			*t_DestChar++ = reinterpret_cast<uint8_t*>(static_cast<uint8_t>(a_Value))[i];
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

		size_t* __restrict  t_intDest = reinterpret_cast<size_t*>(t_Dest);

		while (a_Size >= sizeof(size_t))
		{
			*t_intDest++ = a_Value;
			a_Size -= sizeof(size_t);
		}

		uint8_t* __restrict t_DestChar = reinterpret_cast<uint8_t*>(t_intDest);

		//Again but then go by byte.
		for (size_t i = 0; i < a_Size; i++)
		{
			*t_DestChar++ = reinterpret_cast<uint8_t*>(static_cast<uint8_t>(a_Value))[i];
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
		
		size_t* __restrict  t_intDest = reinterpret_cast<size_t*>(t_Dest);

		while (a_Size >= sizeof(size_t))
		{
			*t_intDest++ = a_Value;
			a_Size -= sizeof(size_t);
		}

		uint8_t* __restrict t_DestChar = reinterpret_cast<uint8_t*>(t_intDest);

		//Again but then go by byte.
		for (size_t i = 0; i < a_Size; i++)
		{
			*t_DestChar++ = reinterpret_cast<uint8_t*>(static_cast<uint8_t>(a_Value))[i];
		}
	}

	bool BB::Memory::MemCmp(const void* __restrict  a_Left, const void* __restrict  a_Right, size_t a_Size)
	{
		const size_t t_Diff = Max(
			Pointer::AlignForwardAdjustment(a_Left, sizeof(size_t)),
			Pointer::AlignForwardAdjustment(a_Right, sizeof(size_t)));

		const uint8_t* __restrict t_AlignLeft = reinterpret_cast<const uint8_t*>(a_Left);
		const uint8_t* __restrict t_AlignRight = reinterpret_cast<const uint8_t*>(a_Right);

		for (size_t i = 0; i < t_Diff; i++)
		{
			if (t_AlignLeft++ != t_AlignRight++)
				return false;
		}

		a_Size -= t_Diff;

		//Get the registry size for most optimal memcpy.
		const size_t* __restrict  t_Left = reinterpret_cast<const size_t*>(t_AlignLeft);
		const size_t* __restrict  t_Right = reinterpret_cast<const size_t*>(t_AlignRight);

		while (a_Size > sizeof(size_t))
		{
			if (t_Left++ != t_Right++)
				return false;
			a_Size -= sizeof(size_t);
		}

		const uint8_t* __restrict  t_CharLeft = reinterpret_cast<const uint8_t*>(t_Right);
		const uint8_t* __restrict  t_CharRight = reinterpret_cast<const uint8_t*>(t_Left);

		while (a_Size--)
		{
			if (t_CharLeft++ != t_CharRight++)
				return false;
		}

		return true;
	}

	bool BB::Memory::MemCmpSIMD128(const void* __restrict  a_Left, const void* __restrict  a_Right, size_t a_Size)
	{
		const size_t t_Diff = Max(
			Pointer::AlignForwardAdjustment(a_Left, sizeof(__m128i)),
			Pointer::AlignForwardAdjustment(a_Right, sizeof(__m128i)));

		const uint8_t* __restrict t_AlignLeft = reinterpret_cast<const uint8_t*>(a_Left);
		const uint8_t* __restrict t_AlignRight = reinterpret_cast<const uint8_t*>(a_Right);

		for (size_t i = 0; i < t_Diff; i++)
		{
			if (t_AlignLeft++ != t_AlignRight++)
				return false;
		}

		a_Size -= t_Diff;

		//Get the registry size for most optimal memcpy.
		const __m128i* __restrict  t_Left = reinterpret_cast<const __m128i*>(t_AlignLeft);
		const __m128i* __restrict  t_Right = reinterpret_cast<const __m128i*>(t_AlignRight);

		const uint64_t t_CmpMode = _SIDD_UBYTE_OPS | _SIDD_CMP_EQUAL_EACH | _SIDD_NEGATIVE_POLARITY | _SIDD_LEAST_SIGNIFICANT;

		while (a_Size >= sizeof(__m128i))
		{
			__m128i loadLeft = _mm_loadu_si128(t_Left++);
			__m128i loadRight = _mm_loadu_si128(t_Right++);
			if (_mm_cmpestrc(loadLeft, static_cast<int>(a_Size), loadRight, static_cast<int>(a_Size), static_cast<int>(t_CmpMode)))
			{
				return false;
			}
			a_Size -= sizeof(__m128i);
		}

		const uint8_t* __restrict  t_CharLeft = reinterpret_cast<const uint8_t*>(t_Right);
		const uint8_t* __restrict  t_CharRight = reinterpret_cast<const uint8_t*>(t_Left);

		while (a_Size--)
		{
			if (t_CharLeft++ != t_CharRight++)
				return false;
		}

		return true;
	}

	bool BB::Memory::MemCmpSIMD256(const void* __restrict a_Left, const void* __restrict a_Right, size_t a_Size)
	{
		const size_t t_Diff = Max(
			Pointer::AlignForwardAdjustment(a_Left, sizeof(__m256i)),
			Pointer::AlignForwardAdjustment(a_Right, sizeof(__m256i)));

		const uint8_t* __restrict t_AlignLeft = reinterpret_cast<const uint8_t*>(a_Left);
		const uint8_t* __restrict t_AlignRight = reinterpret_cast<const uint8_t*>(a_Right);

		for (size_t i = 0; i < t_Diff; i++)
		{
			if (t_AlignLeft++ != t_AlignRight++)
				return false;
		}

		a_Size -= t_Diff;

		const __m256i* __restrict t_Left = reinterpret_cast<const __m256i*>(t_AlignLeft);
		const __m256i* __restrict t_Right = reinterpret_cast<const __m256i*>(t_AlignRight);
		
		for (/**/; a_Size >= sizeof(__m256i); t_Left++, t_Right++)
		{
			const __m256i loadLeft = _mm256_loadu_si256(t_Left);
			const __m256i loadRight = _mm256_loadu_si256(t_Right);
			const __m256i result = _mm256_cmpeq_epi64(loadLeft, loadRight);
			if (!(unsigned int)_mm256_testc_si256(result, _mm256_set1_epi64x(0xFFFFFFFFFFFFFFFF)))
			{
				return false;
			}
			a_Size -= sizeof(__m256i);
		}

		const uint8_t* __restrict  t_CharLeft = reinterpret_cast<const uint8_t*>(t_Right);
		const uint8_t* __restrict  t_CharRight = reinterpret_cast<const uint8_t*>(t_Left);

		while (a_Size--)
		{
			if (t_CharLeft++ != t_CharRight++)
				return false;
		}

		return true;
	}
}