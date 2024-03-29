#pragma once
#include "Common.h"

namespace BB
{
	static inline float ToRadians(const float degrees)
	{
		return degrees * 0.01745329252f;
	}

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

	static inline float2 operator*(const float2 a_Lhs, const float a_Rhs)
	{
		return float2{ a_Lhs.x * a_Rhs, a_Lhs.y * a_Rhs };
	}

	static inline float2 operator/(const float2 a_Lhs, const float2 a_Rhs)
	{
		return float2{ a_Lhs.x / a_Rhs.x, a_Lhs.y / a_Rhs.y };
	}

	// FLOAT2
	//--------------------------------------------------------
	// FLOAT3

	static inline float3 operator+(const float3 a_Lhs, const float3 a_Rhs)
	{
		return float3{ a_Lhs.x + a_Rhs.x, a_Lhs.y + a_Rhs.y, a_Lhs.z + a_Rhs.z };
	}

	static inline float3 operator-(const float3 a_Lhs, const float3 a_Rhs)
	{
		return float3{ a_Lhs.x - a_Rhs.x, a_Lhs.y - a_Rhs.y, a_Lhs.z - a_Rhs.z };
	}

	static inline float3 operator*(const float3 a_Lhs, const float a_Float)
	{
		return float3{ a_Lhs.x * a_Float, a_Lhs.y * a_Float, a_Lhs.z * a_Float };
	}

	static inline float3 operator*(const float3 a_Lhs, const float3 a_Rhs)
	{
		return float3{ a_Lhs.x * a_Rhs.x, a_Lhs.y * a_Rhs.y, a_Lhs.z * a_Rhs.z };
	}

	static inline float3 operator/(const float3 a_Lhs, const float3 a_Rhs)
	{
		return float3{ a_Lhs.x / a_Rhs.x, a_Lhs.y / a_Rhs.y, a_Lhs.z / a_Rhs.z };
	}

	static inline float3 Float3Cross(const float3 a, const float3 b)
	{
		float3 result;
		result.x = a.y * b.z - a.z * b.y;
		result.y = a.z * b.x - a.x * b.z;
		result.z = a.x * b.y - a.y * b.x;
		return result;
	}

	static inline float Float3Dot(const float3 a, const float3 b)
	{
		return a.x * b.x + a.y * b.y + a.z * b.z;
	}

	static inline float Float3LengthSq(const float3 a)
	{
		return Float3Dot(a, a);
	}

	static inline float Float3Length(const float3 a)
	{
		return sqrtf(Float3LengthSq(a));
	}

	static inline float3 Float3Normalize(const float3 a)
	{
		const float length = Float3Length(a);
		const float rcp_length = 1.0f / length;
		return a * rcp_length;
	}

	// FLOAT3
	//--------------------------------------------------------
	// FLOAT4

	static inline float4 operator+(const float4 a_Lhs, const float4 a_Rhs)
	{
		return float4{ a_Lhs.x + a_Rhs.x, a_Lhs.y + a_Rhs.y, a_Lhs.z + a_Rhs.z, a_Lhs.w + a_Rhs.w };
	}

	static inline float4 operator-(const float4 a_Lhs, const float4 a_Rhs)
	{
		return float4{ a_Lhs.x - a_Rhs.x, a_Lhs.y - a_Rhs.y, a_Lhs.z - a_Rhs.z, a_Lhs.w - a_Rhs.w };
	}

	static inline float4 operator*(const float4 a_Lhs, const float a_Float)
	{
		return float4{ a_Lhs.x * a_Float, a_Lhs.y * a_Float, a_Lhs.z * a_Float, a_Lhs.w * a_Float };
	}

	static inline float4 operator*(const float4 a_Lhs, const float4 a_Rhs)
	{
		return float4{ a_Lhs.x * a_Rhs.x, a_Lhs.y * a_Rhs.y, a_Lhs.z * a_Rhs.z, a_Lhs.w * a_Rhs.w };
	}

	static inline float4 operator/(const float4 a_Lhs, const float4 a_Rhs)
	{
		return float4{ a_Lhs.x / a_Rhs.x, a_Lhs.y / a_Rhs.y, a_Lhs.z / a_Rhs.z, a_Lhs.w / a_Rhs.w };
	}

	static inline float Float4Dot(const float4 a, const float4 b)
	{
		return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
	}

	static inline float Float4LengthSq(const float4 a)
	{
		return Float4Dot(a, a);
	}

	static inline float Float4Length(const float4 a)
	{
		return sqrtf(Float4LengthSq(a));
	}

	static inline float4 Float4Normalize(const float4 a)
	{
		const float length = Float4Length(a);
		const float rcp_length = 1.0f / length;
		return a * rcp_length;
	}

	// FLOAT4
	//--------------------------------------------------------
	// MAT4x4

	static inline Mat4x4 Mat4x4FromFloats(
		float m00, float m01, float m02, float m03,
		float m10, float m11, float m12, float m13,
		float m20, float m21, float m22, float m23,
		float m30, float m31, float m32, float m33)
	{
		Mat4x4 mat;
		mat.e[0][0] = m00; mat.e[0][1] = m01; mat.e[0][2] = m02; mat.e[0][3] = m03;
		mat.e[1][0] = m10; mat.e[1][1] = m11; mat.e[1][2] = m12; mat.e[1][3] = m13;
		mat.e[2][0] = m20; mat.e[2][1] = m21; mat.e[2][2] = m22; mat.e[2][3] = m23;
		mat.e[3][0] = m30; mat.e[3][1] = m31; mat.e[3][2] = m32; mat.e[3][3] = m33;
		return mat;
	}

	static inline Mat4x4 Mat4x4FromFloat4s(const float4 r0, const float4 r1, const float4 r2, const float4 r3)
	{
		Mat4x4 mat;
		mat.r0 = r0;
		mat.r1 = r1;
		mat.r2 = r2;
		mat.r3 = r3;
		return mat;
	}

	static inline Mat4x4 operator*(const Mat4x4 a_Lhs, const Mat4x4 a_Rhs)
	{
		Mat4x4 mat;
		mat.r0 = a_Lhs.r0 * a_Rhs.r0.x + a_Lhs.r1 * a_Rhs.r0.y + a_Lhs.r2 * a_Rhs.r0.z + a_Lhs.r3 * a_Rhs.r0.w;
		mat.r1 = a_Lhs.r0 * a_Rhs.r1.x + a_Lhs.r1 * a_Rhs.r1.y + a_Lhs.r2 * a_Rhs.r1.z + a_Lhs.r3 * a_Rhs.r1.w;
		mat.r2 = a_Lhs.r0 * a_Rhs.r2.x + a_Lhs.r1 * a_Rhs.r2.y + a_Lhs.r2 * a_Rhs.r2.z + a_Lhs.r3 * a_Rhs.r2.w;
		mat.r3 = a_Lhs.r0 * a_Rhs.r3.x + a_Lhs.r1 * a_Rhs.r3.y + a_Lhs.r2 * a_Rhs.r3.z + a_Lhs.r3 * a_Rhs.r3.w;
		return mat;
	}

	static inline Mat4x4 Mat4x4Identity()
	{
		Mat4x4 mat = { 0 };
		mat.e[0][0] = 1;
		mat.e[1][1] = 1;
		mat.e[2][2] = 1;
		mat.e[3][3] = 1;
		return mat;
	}

	static inline Mat4x4 Mat4x4FromTranslation(const float3 translation)
	{
		Mat4x4 result = Mat4x4Identity();
		result.e[3][0] = translation.x;
		result.e[3][1] = translation.y;
		result.e[3][2] = translation.z;
		return result;
	}

	static inline Mat4x4 Mat4x4FromQuat(const Quat q)
	{
		Mat4x4 rotMat = Mat4x4Identity();
		const float qxx = q.x * q.x;
		const float qyy = q.y * q.y;
		const float qzz = q.z * q.z;

		const float qxz = q.x * q.z;
		const float qxy = q.x * q.y;
		const float qyz = q.y * q.z;

		const float qwx = q.w * q.x;
		const float qwy = q.w * q.y;
		const float qwz = q.w * q.z;

		rotMat.e[0][0] = 1.f - 2.f * (qyy + qzz);
		rotMat.e[0][1] = 2.f * (qxy + qwz);
		rotMat.e[0][2] = 2.f * (qxz - qwy);

		rotMat.e[1][0] = 2.f * (qxy - qwz);
		rotMat.e[1][1] = 1.f - 2.f * (qxx + qzz);
		rotMat.e[1][2] = 2.f * (qyz + qwx);

		rotMat.e[2][0] = 2.f * (qxz + qwy);
		rotMat.e[2][1] = 2.f * (qyz - qwx);
		rotMat.e[2][2] = 1.f - 2.f * (qxx + qyy);
		return rotMat;
	}

	static inline Mat4x4 Mat4x4Scale(const Mat4x4 m, const float3 s)
	{
		Mat4x4 mat;
		mat.r0 = m.r0 * s.x;
		mat.r1 = m.r1 * s.y;
		mat.r2 = m.r2 * s.z;
		mat.r3 = m.r3;
		return mat;
	}

	static Mat4x4 Mat4x4Perspective(const float fov, const float aspect, const float nearField, const float farField)
	{
		const float tanHalfFov = tan(fov / 2.f);

		Mat4x4 mat = {0};
		mat.e[0][0] = 1.f / (aspect * tanHalfFov);
		mat.e[1][1] = 1.f / (tanHalfFov);
		mat.e[2][2] = -(farField + nearField) / (farField - nearField);
		mat.e[2][3] = -1.f;
		mat.e[3][2] = -(2.f * farField * nearField) / (farField - nearField);
		return mat;
	}

	static inline Mat4x4 Mat4x4Lookat(const float3 eye, const float3 center, const float3 up)
	{
		const float3 f = Float3Normalize(center - eye);
		const float3 s = Float3Normalize(Float3Cross(f, up));
		const float3 u = Float3Cross(s, f);

		Mat4x4 mat = Mat4x4Identity();
		mat.e[0][0] = s.x;
		mat.e[1][0] = s.y;
		mat.e[2][0] = s.z;
		mat.e[0][1] = u.x;
		mat.e[1][1] = u.y;
		mat.e[2][1] = u.z;
		mat.e[0][2] = -f.x;
		mat.e[1][2] = -f.y;
		mat.e[2][2] = -f.z;
		mat.e[3][0] = -Float3Dot(s, eye);
		mat.e[3][1] = -Float3Dot(u, eye);
		mat.e[3][2] = Float3Dot(f, eye);
		return mat;
	}

	static inline Mat4x4 Mat4x4Inverse(const Mat4x4 m)
	{
		const float3 a = float3{ m.e[0][0], m.e[1][0], m.e[2][0] };
		const float3 b = float3{ m.e[0][1], m.e[1][1], m.e[2][1] };
		const float3 c = float3{ m.e[0][2], m.e[1][2], m.e[2][2] };
		const float3 d = float3{ m.e[0][3], m.e[1][3], m.e[2][3] };

		const float x = m.e[3][0];
		const float y = m.e[3][1];
		const float z = m.e[3][2];
		const float w = m.e[3][3];

		float3 s = Float3Cross(a, b);
		float3 t = Float3Cross(c, d);

		float3 u = (a * y) - (b * x);
		float3 v = (c * w) - (d * z);

		float inv_det = 1.0f / (Float3Dot(s, v) + Float3Dot(t, u));
		s = (s * inv_det);
		t = (t * inv_det);
		u = (u * inv_det);
		v = (v * inv_det);

		const float3 r0 = (Float3Cross(b, v) + (t * y));
		const float3 r1 = (Float3Cross(v, a) - (t * x));
		const float3 r2 = (Float3Cross(d, u) + (s * w));
		const float3 r3 = (Float3Cross(u, c) - (s * z));

		return Mat4x4FromFloats(
			r0.x, r0.y, r0.z, -Float3Dot(b, t),
			r1.x, r1.y, r1.z, Float3Dot(a, t),
			r2.x, r2.y, r2.z, -Float3Dot(d, s),
			r3.x, r3.y, r3.z, Float3Dot(c, s));
	}

	// MAT4x4
	//--------------------------------------------------------
	// QUAT

	static inline Quat operator*(const Quat a_Lhs, const Quat a_Rhs)
	{
		return Quat{ a_Lhs.x * a_Rhs.x, a_Lhs.y * a_Rhs.y, a_Lhs.z * a_Rhs.z, a_Lhs.w * a_Rhs.w };
	}

	static inline Quat IdentityQuat()
	{
		return Quat{ 0, 0, 0, 1 };
	}

	static inline Quat QuatFromAxisAngle(const float3 axis, const float angle)
	{
		//const float3 normAxis = Float3Normalize(axis);

		const float s = sinf(0.5f * angle);

		Quat quat;
		quat.xyz = axis * s;
		quat.w = cosf(angle * 0.5f);
		return quat;
	}

	static inline Quat QuatRotateQuat(const Quat a, const Quat b)
	{
		Quat quat;
		quat.x = a.x * b.x;
		quat.y = a.y * b.y;
		quat.z = a.z * b.z;
		quat.w = a.w * b.w;
		return quat;
	}
}