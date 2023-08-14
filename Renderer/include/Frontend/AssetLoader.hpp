#include "BBMemory.h"
#include "RenderBackendCommon.h"

namespace BB
{
	using AssetHandle = FrameworkHandle<struct AssetHandleTag>;



	enum class AssetType : uint32_t
	{
		IMAGE
	};

	enum class AssetLoadType : uint32_t
	{
		DISK,
		MEMORY
	};

	struct AssetDiskJobInfo
	{
		AssetType assetType;
		AssetLoadType loadType;
		const char* path;
	};

	namespace Asset
	{
		AssetHandle LoadAsset(void* a_AssetJobInfo);

		const RImageHandle GetImage(const AssetHandle a_Asset);
		const RImageHandle GetImageWait(const char* a_Path);
	};
}