#pragma once
#include "Allocators/AllocTypes.h"

namespace BB
{
	class Framework
	{
	public:
		Framework();
		~Framework();

		void Init();

		void* MainWindow() const { return m_MainWindow; }

	private:
		FreeListAllocator_t m_Allocator{ mbSize * 4 };
		LinearAllocator_t m_TempAllocator{ mbSize * 4 };

		void* m_MainWindow = nullptr;
	};
}
