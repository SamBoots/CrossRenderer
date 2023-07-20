#include "BBMemory.h"

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

		bool IsFinished();

	private:
		void LoadTexture(const AssetLoaderInfo& a_Info);
		LinearAllocator_t m_Allocator{ mbSize * 4 };
		struct AssetLoader_inst* inst;
	}
}