#include "BBMemory.h"
#include "RenderBackendCommon.h"

namespace BB
{
	class AssetLoader
	{
	public:
		AssetLoader();
		~AssetLoader();

		RImageHandle LoadImage(const char* a_Path);

		bool IsFinished() const { return m_IsFinished; };

	private:
		LinearAllocator_t m_Allocator{ mbSize * 4 };

		uint64_t m_WaitValue = UINT64_MAX;
		bool m_IsFinished = false;
	};
}