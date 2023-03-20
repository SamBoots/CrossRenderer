#pragma once
#include <cstdint>

namespace BB
{
	template<typename Tag>
	union FrameworkHandle
	{
		FrameworkHandle() {};
		FrameworkHandle(void* a_Handle)
		{
			ptrHandle = a_Handle;
		};
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
			//A extra handle index, can be used to track something else. Usually this value is 0.
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

	using wchar = wchar_t;

	struct Buffer
	{
		void* data;
		uint64_t size;
	};


	struct float2
	{
		float x = 0;
		float y = 0;
	};

	struct float3
	{
		float x = 0;
		float y = 0;
		float z = 0;
	};
	struct float4
	{
		float x = 0;
		float y = 0;
		float z = 0;
		float w = 0;
	};

	struct int2
	{
		int x = 0;
		int y = 0;
	};

	struct int3
	{
		int x = 0;
		int y = 0;
		int z = 0;
	};

	struct int4
	{
		int x = 0;
		int y = 0;
		int z = 0;
		int w = 0;
	};

	struct uint2
	{
		uint32_t x = 0;
		uint32_t y = 0;
	};

	struct uint3
	{
		uint32_t x = 0;
		uint32_t y = 0;
		uint32_t z = 0;
	};

	struct uint4
	{
		uint32_t x = 0;
		uint32_t y = 0;
		uint32_t z = 0;
		uint32_t w = 0;
	};
}