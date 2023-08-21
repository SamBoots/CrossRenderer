#include "BBMemory.h"
#include "RenderBackendCommon.h"

namespace BB
{
	using AssetHandle = FrameworkHandle<struct AssetHandleTag>;
	constexpr const char JSON_DIRECTORY[] = "Resources/Json/";
	constexpr const char MODELS_DIRECTORY[] = "Resources/Models/";
	constexpr const char SHADERS_DIRECTORY[] = "Resources/Shaders/";
	constexpr const char TEXTURE_DIRECTORY[] = "Resources/Textures/";


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
		char* FindOrCreateString(const char* a_string);

		const AssetHandle LoadAsset(void* a_AssetJobInfo);

		const RImageHandle GetImage(const AssetHandle a_Asset);
		const RImageHandle GetImageWait(const char* a_Path);
	};
}