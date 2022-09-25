#pragma once
#include "Utils/Logger.h"
#include <cstdint>
#include <cmath>
#include <cstring>

#include <cwchar>

namespace BB
{
	namespace Memory	
	{

		void MemCpy(void* __restrict  a_Destination, const void* __restrict  a_Source, size_t a_Size);

		/// <summary>
		/// Memcpy abstraction that will call the constructor if needed.
		/// </summary>
		template<typename T>
		inline static void Copy(T* __restrict a_Destination, const T* __restrict a_Source, const size_t a_ElementCount)
		{
			if constexpr (std::is_trivially_constructible_v<T>)
			{
				memcpy(a_Destination, a_Source, a_ElementCount * sizeof(T));
			}
			else
			{
				for (size_t i = 0; i < a_ElementCount; i++)
				{
					new (&a_Destination[i]) T(a_Source[i]);
				}
			}
		}

		/// <summary>
		/// Memcpy abstraction that will call the constructor and/or deconstructor if needed.
		/// Unsafe version: It will call memcpy instead of memmove
		/// </summary>
		template<typename T>
		inline static void* Move(T* __restrict a_Destination, const T* __restrict a_Source, const size_t a_ElementCount)
		{
			constexpr bool trivalConstruction = std::is_trivially_constructible_v<T>;
			constexpr bool trivalDestructible = std::is_trivially_destructible_v<T>;

			if constexpr (trivalConstruction)
			{
				return memcpy(a_Destination, a_Source, a_ElementCount * sizeof(T));
			}
			else if constexpr (!trivalConstruction && !trivalDestructible)
			{
				for (size_t i = 0; i < a_ElementCount; i++)
				{
					if constexpr (!trivalConstruction)
					{
						new (&a_Destination[i]) T(a_Source[i]);
					}
					if constexpr (!trivalDestructible)
					{
						a_Source[i].~T();
					}
				}
				return a_Destination;
			}
			BB_ASSERT(false, "Something weird happened in Utils.h, Unsafe Move.");
			return nullptr;
		}

		/// <summary>
		/// Memmove abstraction that will call the constructor and/or deconstructor if needed.
		/// Safe version: It will call memmove instead of memcpy
		/// </summary>
		template<typename T>
		inline static void* sMove(T* __restrict a_Destination, const T* __restrict a_Source, const size_t a_ElementCount)
		{
			constexpr bool trivalConstruction = std::is_trivially_constructible_v<T>;
			constexpr bool trivalDestructible = std::is_trivially_destructible_v<T>;

			if constexpr (trivalConstruction)
			{
				return memmove(a_Destination, a_Source, a_ElementCount * sizeof(T));
			}
			else if constexpr (!trivalConstruction && !trivalDestructible)
			{
				for (size_t i = 0; i < a_ElementCount; i++)
				{
					if constexpr (!trivalConstruction)
					{
						new (&a_Destination[i]) T(a_Source[i]);
					}
					if constexpr (!trivalDestructible)
					{
						a_Source[i].~T();
					}
				}
				return a_Destination;
			}
			BB_ASSERT(false, "Something weird happened in Utils.h, Unsafe Move.");
			return nullptr;
		}

		/// <summary>
		/// memset abstraction that will use the sizeof operator for type T.
		/// </summary>
		template<typename T>
		inline static void* Set(T* __restrict a_Destination, const int a_Value, const size_t a_ElementCount)
		{
			return memset(a_Destination, a_Value, a_ElementCount * sizeof(T));
		}

		/// <summary>
		/// memcmp abstraction that will use the sizeof operator for type T.
		/// </summary>
		template<typename T>
		inline static int Compare(T* __restrict a_Destination, const void* __restrict a_Source, const size_t a_ElementCount)
		{
			return memcmp(a_Destination, a_Source, a_ElementCount * sizeof(T));
		}

		inline static size_t StrLength(const char* a_String)
		{
			return strlen(a_String);
		}

		inline static size_t StrLength(const wchar_t* a_String)
		{
			return wcslen(a_String);
		}
	}

	namespace Math
	{
		inline static size_t RoundUp(const size_t a_NumToRound, const size_t a_Multiple)
		{
			return ((a_NumToRound + a_Multiple - 1) / a_Multiple) * a_Multiple;
		}

		inline static size_t Max(const size_t a_A, const size_t a_B)
		{
			if (a_A > a_B)
				return a_A;
			return a_B;
		}

		inline static size_t Min(const size_t a_A, const size_t a_B)
		{
			if (a_A < a_B)
				return a_A;
			return a_B;
		}

		inline static float Lerp(const float a_A, const float a_B, const float a_T)
		{
			return a_A + a_T * (a_B - a_A);
		}

		inline static int clamp(int a_Value, int a_Min, int a_Max)
		{
			if (a_Value > a_Max)
			{
				return a_Max;
			}
			else if (a_Value < a_Min)
			{
				return a_Min;
			}
			return a_Value;
		}
	}

	namespace Random
	{
		static unsigned int MathRandomSeed = 1;

		/// <summary>
		/// Set the random seed that the Math.h header uses.
		/// </summary>
		inline static void Seed(const unsigned int a_Seed)
		{
			MathRandomSeed = a_Seed;
		}

		/// <summary>
		/// Get a Random unsigned int between 0 and INT_MAX.
		/// </summary>
		inline static unsigned int Random()
		{
			MathRandomSeed ^= MathRandomSeed << 13, MathRandomSeed ^= MathRandomSeed >> 17;
			MathRandomSeed ^= MathRandomSeed << 5;
			return MathRandomSeed;
		}

		/// <summary>
		/// Get a Random unsigned int between 0 and maxValue.
		/// </summary>
		inline static  unsigned int Random(const unsigned int a_Max)
		{
			return Random() % a_Max;
		}

		/// <summary>
		/// Get a Random unsigned int between min and max value.
		/// </summary>
		inline static unsigned int Random(const unsigned int a_Min, const unsigned int a_Max)
		{
			return Random() % (a_Max + 1 - a_Min) + a_Min;
		}

		/// <summary>
		/// Get a Random float between 0 and 1.
		/// </summary>
		inline static float RandomF()
		{
			return fmod(static_cast<float>(Random()) * 2.3283064365387e-10f, 1.0f);
		}

		/// <summary>
		/// Get a Random float between 0 and max value.
		/// </summary>
		inline static  float RandomF(const float a_Min, const float a_Max)
		{
			return (RandomF() * (a_Max - a_Min)) + a_Min;
		}
	}

	namespace Pointer
	{
		/// <summary>
		/// Move the given pointer by a given size.
		/// </summary>
		/// <param name="a_Ptr:"> The pointer you want to shift </param>
		/// <param name="a_Add:"> The amount of bytes you want move the pointer forward. </param>
		/// <returns>The shifted pointer. </returns>
		inline static void* Add(const void* a_Ptr, const size_t a_Add)
		{
			return reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(a_Ptr) + a_Add);
		}

		/// <summary>
		/// Move the given pointer by a given size.
		/// </summary>
		/// <param name="a_Ptr:"> The pointer you want to shift </param>
		/// <param name="a_Subtract:"> The amount of bytes you want move the pointer backwards. </param>
		/// <returns>The shifted pointer. </returns>
		inline static void* Subtract(const void* a_Ptr, const size_t a_Subtract)
		{
			return reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(a_Ptr) - a_Subtract);
		}

		/// <summary>
		/// Align a given pointer forward.
		/// </summary>
		/// <param name="a_Ptr:"> The pointer you want to align </param>
		/// <param name="a_Alignment:"> The alignment of the data. </param>
		/// <returns>The given address but aligned forward. </returns>
		inline static size_t AlignForwardAdjustment(const void* a_Ptr, const size_t a_Alignment)
		{
			size_t adjustment = a_Alignment - (reinterpret_cast<uintptr_t>(a_Ptr) & static_cast<uintptr_t>(a_Alignment - 1));

			if (adjustment == a_Alignment) return 0;

			//already aligned 
			return adjustment;
		}

		/// <summary>
		/// Align a given pointer forward.
		/// </summary>
		/// <param name="a_Ptr:"> The pointer you want to align </param>
		/// <param name="a_Alignment:"> The alignment of the data. </param>
		/// <param name="a_HeaderSize:"> The size in bytes of the Header you want to align forward's too </param>
		/// <returns>The given address but aligned forward with the allocation header's size in mind. </returns>
		inline static size_t AlignForwardAdjustmentHeader(const void* a_Ptr, const size_t a_Alignment, const size_t a_HeaderSize)
		{
			size_t adjustment = AlignForwardAdjustment(a_Ptr, a_Alignment);
			size_t neededSpace = a_HeaderSize;

			if (adjustment < neededSpace)
			{
				neededSpace -= adjustment;

				//Increase adjustment to fit header 
				adjustment += a_Alignment * (neededSpace / a_Alignment);

				if (neededSpace % a_Alignment > 0) adjustment += a_Alignment;
			}

			return adjustment;
		}
	}
}