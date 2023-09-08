#pragma once
#include "Common.h"

namespace BB
{
	namespace Threads
	{
		void InitThreads(const uint32_t a_ThreadCount);
		void DestroyThreads();
		ThreadTask StartTaskThread(void(*a_Function)(void*), void* a_FuncParameter);

		void WaitForTask(const ThreadTask a_Handle);
		bool TaskFinished(const ThreadTask a_Handle);
	}
}