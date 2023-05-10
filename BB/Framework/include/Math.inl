#pragma once
#include "Common.h"

namespace BB
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

	inline static int Clamp(const int a_Value, const int a_Min, const int a_Max)
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

	inline static float Clampf(const float a_Value, const float a_Min, const float a_Max)
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

	static inline float2 operator+(const float2 a_Lhs, const float2 a_Rhs)
	{
		return float2{ a_Lhs.x + a_Rhs.x, a_Lhs.y + a_Rhs.y };
	}

	static inline float2 operator-(const float2 a_Lhs, const float2 a_Rhs)
	{
		return float2{ a_Lhs.x - a_Rhs.x, a_Lhs.y - a_Rhs.y };
	}

	static inline float2 operator*(const float2 a_Lhs, const float2 a_Rhs)
	{
		return float2{ a_Lhs.x * a_Rhs.x, a_Lhs.y * a_Rhs.y };
	}

	static inline float2 operator/(const float2 a_Lhs, const float2 a_Rhs)
	{
		return float2{ a_Lhs.x / a_Rhs.x, a_Lhs.y / a_Rhs.y };
	}

	static inline float3 operator+(const float3 a_Lhs, const float3 a_Rhs)
	{
		return float3{ a_Lhs.x + a_Rhs.x, a_Lhs.y + a_Rhs.y, a_Lhs.z + a_Rhs.z };
	}

	static inline float3 operator-(const float3 a_Lhs, const float3 a_Rhs)
	{
		return float3{ a_Lhs.x - a_Rhs.x, a_Lhs.y - a_Rhs.y, a_Lhs.z - a_Rhs.z };
	}

	static inline float3 operator*(const float3 a_Lhs, const float3 a_Rhs)
	{
		return float3{ a_Lhs.x * a_Rhs.x, a_Lhs.y * a_Rhs.y, a_Lhs.z * a_Rhs.z };
	}

	static inline float3 operator/(const float3 a_Lhs, const float3 a_Rhs)
	{
		return float3{ a_Lhs.x / a_Rhs.x, a_Lhs.y / a_Rhs.y, a_Lhs.z / a_Rhs.z };
	}

	static inline float4 operator+(const float4 a_Lhs, const float4 a_Rhs)
	{
		return float4{ a_Lhs.x + a_Rhs.x, a_Lhs.y + a_Rhs.y, a_Lhs.z + a_Rhs.z, a_Lhs.w + a_Rhs.w };
	}

	static inline float4 operator-(const float4 a_Lhs, const float4 a_Rhs)
	{
		return float4{ a_Lhs.x - a_Rhs.x, a_Lhs.y - a_Rhs.y, a_Lhs.z - a_Rhs.z, a_Lhs.w - a_Rhs.w };
	}

	static inline float4 operator*(const float4 a_Lhs, const float4 a_Rhs)
	{
		return float4{ a_Lhs.x * a_Rhs.x, a_Lhs.y * a_Rhs.y, a_Lhs.z * a_Rhs.z, a_Lhs.w * a_Rhs.w };
	}

	static inline float4 operator/(const float4 a_Lhs, const float4 a_Rhs)
	{
		return float4{ a_Lhs.x / a_Rhs.x, a_Lhs.y / a_Rhs.y, a_Lhs.z / a_Rhs.z, a_Lhs.w / a_Rhs.w };
	}
}