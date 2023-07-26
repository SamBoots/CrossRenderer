#pragma once
#include "Common.h"

namespace BB
{
	using ThreadHandle = FrameworkHandle<struct ThreadHandletag>;

	namespace Threads
	{
		void InitThreads(const uint32_t a_ThreadCount);
		ThreadHandle StartThread(void(*a_Function)(void*), void* a_FuncParameter);
		void WaitForThread(const ThreadHandle a_Handle);
	}
}