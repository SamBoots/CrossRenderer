#include "BBThreadScheduler.hpp"
#include "Program.h"

using namespace BB;

enum class THREAD_STATUS : uint32_t
{
	IDLE,
	BUSY,
	DESTROY
};

struct ThreadInfo
{
	void(*function)(void*);
	void* functionParameter;
	THREAD_STATUS threadStatus;
	//debug value for the ThreadHandle extraIndex;
	uint32_t generation;
};

static void ThreadStartFunc(void* a_Args)
{
	ThreadInfo* t_ThreadInfo = reinterpret_cast<ThreadInfo*>(a_Args);
	while (t_ThreadInfo->threadStatus != THREAD_STATUS::DESTROY)
	{
		if (t_ThreadInfo->function != nullptr)
		{
			t_ThreadInfo->threadStatus = THREAD_STATUS::BUSY;
			t_ThreadInfo->function(t_ThreadInfo->functionParameter);
			++t_ThreadInfo->generation;
			t_ThreadInfo->function = nullptr;
			t_ThreadInfo->functionParameter = nullptr;
			t_ThreadInfo->threadStatus = THREAD_STATUS::IDLE;
		}
	}
}

struct Thread
{
	OSThreadHandle osThreadHandle;
	ThreadInfo threadInfo; //used to send functions to threads
};

struct ThreadScheduler
{
	uint32_t threadCount = 0;
	Thread threads[32]{};
};

static ThreadScheduler s_ThreadScheduler;

void BB::Threads::InitThreads(const uint32_t a_ThreadCount)
{
	BB_ASSERT(a_ThreadCount < _countof(s_ThreadScheduler.threads), "Trying to create too many threads!");
	s_ThreadScheduler.threadCount = a_ThreadCount;

	for (uint32_t i = 0; i < s_ThreadScheduler.threadCount; i++)
	{
		s_ThreadScheduler.threads[i].threadInfo.function = nullptr;
		s_ThreadScheduler.threads[i].threadInfo.functionParameter = nullptr;
		s_ThreadScheduler.threads[i].threadInfo.threadStatus = THREAD_STATUS::IDLE;
		s_ThreadScheduler.threads[i].threadInfo.generation = 0;
		s_ThreadScheduler.threads[i].osThreadHandle = OSCreateThread(ThreadStartFunc,
			0,
			&s_ThreadScheduler.threads[i].threadInfo);
	}
}

void BB::Threads::DestroyThreads()
{
	for (uint32_t i = 0; i < s_ThreadScheduler.threadCount; i++)
	{
		s_ThreadScheduler.threads[i].threadInfo.threadStatus = THREAD_STATUS::DESTROY;
	}
}

ThreadTask BB::Threads::StartTaskThread(void(*a_Function)(void*), void* a_FuncParameter)
{
	for (uint32_t i = 0; i < s_ThreadScheduler.threadCount; i++)
	{
		if (s_ThreadScheduler.threads[i].threadInfo.threadStatus == THREAD_STATUS::IDLE)
		{
			s_ThreadScheduler.threads[i].threadInfo.function = a_Function;
			s_ThreadScheduler.threads[i].threadInfo.functionParameter = a_FuncParameter;
			return ThreadTask(i, s_ThreadScheduler.threads[i].threadInfo.generation + 1);
		}
	}
	BB_ASSERT(false, "No free threads! Maybe implement a way to just re-iterate over the list again.");
}

void BB::Threads::WaitForTask(const ThreadTask a_Handle)
{
	while (s_ThreadScheduler.threads->threadInfo.generation > a_Handle.extraIndex) {};
}

bool BB::Threads::TaskFinished(const ThreadTask a_Handle)
{
	if (s_ThreadScheduler.threads->threadInfo.generation >= a_Handle.extraIndex)
		return true;

	return false;
}