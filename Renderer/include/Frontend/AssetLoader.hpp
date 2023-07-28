#include "BBMemory.h"
#include "RenderBackendCommon.h"

namespace BB
{
	enum class ASSET_TYPE
	{
		GLTF,
		TEXTURE
	};

	struct AssetLoaderInfo
	{
		ASSET_TYPE assetType;
		const char* path;
	};

	class AssetLoader
	{
	public:
		AssetLoader(const AssetLoaderInfo& a_Info);
		~AssetLoader();

		bool IsFinished() const { return m_IsFinished; };

	private:
		LinearAllocator_t m_Allocator{ mbSize * 4 };

		CommandAllocatorHandle m_CmdAllocator;
		CommandListHandle m_CommandList;
		uint64_t m_WaitValue = UINT64_MAX;
		bool m_IsFinished = false;
	};
}