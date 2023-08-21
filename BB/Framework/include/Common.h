#pragma once
#include <cstdint>

namespace BB
{
//Thank you Descent Raytracer teammates great code that I can steal
#define BB_SLL_PUSH(head, node) ((node)->next = (head), (head) = (node))
#define BB_SLL_POP(head) head; do { (head) = (head)->next; } while(0)

	constexpr const uint64_t BB_INVALID_HANDLE = 0;

	template<typename Tag>
	union FrameworkHandle
	{
		FrameworkHandle() {};
		FrameworkHandle(uint64_t a_Handle)
		{
			handle = a_Handle;
		};
		FrameworkHandle(uint32_t a_Index, uint32_t a_ExtraIndex)
		{
			index = a_Index;
			extraIndex = a_ExtraIndex;
		};
		struct
		{
			//The handle's main index. Always used and is the main handle.
			uint32_t index;
			//A extra handle index, can be used to track something else. Usually this value is 0 or is part of a pointer.
			uint32_t extraIndex;
		};
		//Some handles work with pointers.
		void* ptrHandle;
		uint64_t handle{};

		inline bool operator ==(FrameworkHandle a_Rhs) const { return handle == a_Rhs.handle; }
		inline bool operator !=(FrameworkHandle a_Rhs) const { return handle != a_Rhs.handle; }
	};

	using WindowHandle = FrameworkHandle<struct WindowHandleTag>;
	//A handle to a loaded lib/dll from OS::LoadLib and can be destroyed using OS::UnloadLib
	using LibHandle = FrameworkHandle<struct LibHandleTag>;
	using OSFileHandle = FrameworkHandle<struct OSFileHandleTag>;
	using BBMutex = FrameworkHandle<struct BBMutexTag>;

	using wchar = wchar_t;

	struct Buffer
	{
		char* data;
		uint64_t size;
	};


	union float2
	{
		float e[2];
		struct
		{
			float x;
			float y;
		};
	};

	union float3
	{
		float e[3];
		struct
		{
			float x;
			float y;
			float z;
		};
	};

	union float4
	{
		float e[4];
		struct
		{
			float x;
			float y;
			float z;
			float w;
		};
	};

	union int2
	{
		int e[2];
		struct
		{
			int x;
			int y;
		};
	};

	union int3
	{
		int e[3];
		struct
		{
			int x;
			int y;
			int z;
		};
	};

	union int4
	{
		int e[4];
		struct
		{
			int x;
			int y;
			int z;
			int w;
		};
	};

	union uint2
	{
		uint32_t e[2];
		struct
		{
			uint32_t x;
			uint32_t y;
		};
	};

	union uint3
	{
		uint32_t e[3];
		struct
		{
			uint32_t x;
			uint32_t y;
			uint32_t z;
		};
	};

	union uint4
	{
		uint32_t e[4];
		struct
		{
			uint32_t x;
			uint32_t y;
			uint32_t z;
			uint32_t w;
		};
	};

	union Quat
	{
		struct
		{
			float x;
			float y;
			float z;
			float w;
		};
		float3 xyz;
		float4 xyzw;
	};

	union Mat4x4
	{
		float e[4][4];
		struct
		{
			float4 r0;
			float4 r1;
			float4 r2;
			float4 r3;
		};
	};
}