#include "AssetLoader.hpp"
#include "RenderFrontend.h"
#include "Storage/BBString.h"
#include "Hash.h"


#pragma warning(push, 0)
#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"
#pragma warning (pop)

#include "Storage/Hashmap.h"

using namespace BB;

struct AssetSlot
{
	AssetType type;
	Hash hash;
	char path[256];
	union
	{
		RImageHandle image{};
	};
};

struct AssetManager
{
	FreelistAllocator_t assetAllocator{ mbSize * 64, "asset manager allocator" };
	OL_HashMap<Hash, AssetSlot> assetMap{ assetAllocator, 64 };
};
static AssetManager s_AssetManager{};

using namespace BB;

static CommandList* SetupCommandLists(const char* a_Name = "default asset loader name")
{
	CommandList* t_CmdList = Render::GetTransferQueue().GetCommandList();

	//TODO, debug name the resource here.
	StackString<256> t_DebugNames(a_Name);
	constexpr const char* COMMAND_ALLOC_NAME = " : command allocator";
	constexpr const char* COMMAND_LIST_NAME = " : command list";

	t_DebugNames.append(COMMAND_ALLOC_NAME);
	//m_CmdAllocator = RenderBackend::CreateCommandAllocator(t_CreateInfo);
	t_DebugNames.pop_back(strlen(COMMAND_ALLOC_NAME));

	t_DebugNames.append(COMMAND_LIST_NAME);
	//m_CommandList = RenderBackend::CreateCommandList(t_CreateInfo);

	return t_CmdList;
}

static RImageHandle LoadImage(const char* a_Path)
{
	CommandList* t_CmdList = SetupCommandLists(a_Path);

	int x, y, c;
	//hacky way, whatever we do it for now.
	stbi_uc* t_Pixels = stbi_load(a_Path, &x, &y, &c, 4);
	RImageHandle t_Image;
	{
		RenderImageCreateInfo t_ImageInfo;
		t_ImageInfo.name = a_Path;
		t_ImageInfo.width = static_cast<uint32_t>(x);
		t_ImageInfo.height = static_cast<uint32_t>(y);
		t_ImageInfo.depth = 1;
		t_ImageInfo.arrayLayers = 1;
		t_ImageInfo.mipLevels = 1;
		t_ImageInfo.tiling = RENDER_IMAGE_TILING::OPTIMAL;
		t_ImageInfo.type = RENDER_IMAGE_TYPE::TYPE_2D;
		t_ImageInfo.format = RENDER_IMAGE_FORMAT::RGBA8_SRGB;
		t_Image = RenderBackend::CreateImage(t_ImageInfo);

		//Transfer image to prepare for transfer
		PipelineBarrierImageInfo t_ImageTransInfo{};
		t_ImageTransInfo.srcMask = RENDER_ACCESS_MASK::NONE;
		t_ImageTransInfo.dstMask = RENDER_ACCESS_MASK::TRANSFER_WRITE;
		t_ImageTransInfo.image = t_Image;
		t_ImageTransInfo.oldLayout = RENDER_IMAGE_LAYOUT::UNDEFINED;
		t_ImageTransInfo.newLayout = RENDER_IMAGE_LAYOUT::TRANSFER_DST;
		t_ImageTransInfo.layerCount = 1;
		t_ImageTransInfo.levelCount = 1;
		t_ImageTransInfo.baseArrayLayer = 0;
		t_ImageTransInfo.baseMipLevel = 0;
		t_ImageTransInfo.srcStage = RENDER_PIPELINE_STAGE::TOP_OF_PIPELINE;
		t_ImageTransInfo.dstStage = RENDER_PIPELINE_STAGE::TRANSFER;

		PipelineBarrierInfo t_Barrier{};
		t_Barrier.imageInfoCount = 1;
		t_Barrier.imageInfos = &t_ImageTransInfo;
		RenderBackend::SetPipelineBarriers(t_CmdList->list, t_Barrier);
	}

	const ImageReturnInfo t_ImageInfo = RenderBackend::GetImageInfo(t_Image);
	constexpr size_t TEXTURE_BYTE_ALIGNMENT = 512;
	//NEED TO DO ALIGNMENT ON THE IMAGE IF WE USE THE UPLOAD BUFFER FOR MORE THEN JUST ONE IMAGE.
	UploadBuffer t_ImageUpload(t_ImageInfo.allocInfo.imageAllocByteSize);
	{
		const uint64_t t_SourcePitch = static_cast<uint64_t>(t_ImageInfo.width) * sizeof(uint32_t);

		void* t_ImageSrc = t_Pixels;
		void* t_ImageDst = t_ImageUpload.GetStart();
		//Layouts should be only 1 right now due to mips.
		for (uint32_t i = 0; i < t_ImageInfo.allocInfo.footHeight; i++)
		{
			memcpy(t_ImageDst, t_ImageSrc, t_SourcePitch);

			t_ImageSrc = Pointer::Add(t_ImageSrc, t_SourcePitch);
			t_ImageDst = Pointer::Add(t_ImageDst, t_ImageInfo.allocInfo.footRowPitch);
		}
	}
	STBI_FREE(t_Pixels);

	{	//copy buffer to image
		RenderCopyBufferImageInfo t_CopyImageInfo{};
		t_CopyImageInfo.srcBuffer = t_ImageUpload.Buffer();
		t_CopyImageInfo.srcBufferOffset = 0;
		t_CopyImageInfo.dstImage = t_Image;
		t_CopyImageInfo.dstImageInfo.sizeX = static_cast<uint32_t>(x);
		t_CopyImageInfo.dstImageInfo.sizeY = static_cast<uint32_t>(y);
		t_CopyImageInfo.dstImageInfo.sizeZ = 1;
		t_CopyImageInfo.dstImageInfo.offsetX = 0;
		t_CopyImageInfo.dstImageInfo.offsetY = 0;
		t_CopyImageInfo.dstImageInfo.offsetZ = 0;
		t_CopyImageInfo.dstImageInfo.layerCount = 1;
		t_CopyImageInfo.dstImageInfo.mipLevel = 0;
		t_CopyImageInfo.dstImageInfo.baseArrayLayer = 0;
		t_CopyImageInfo.dstImageInfo.layout = RENDER_IMAGE_LAYOUT::TRANSFER_DST;

		RenderBackend::CopyBufferImage(t_CmdList->list, t_CopyImageInfo);
	}

	RenderBackend::EndCommandList(t_CmdList->list);

	RenderQueue& t_TransferQueue = Render::GetTransferQueue();
	const uint64_t t_WaitValue = t_TransferQueue.GetNextFenceValue();

	t_TransferQueue.ExecuteCommands(&t_CmdList, 1, nullptr, nullptr, 0);

	//for now just stall the thread.
	t_TransferQueue.WaitFenceValue(t_WaitValue);

	return t_Image;
}

AssetHandle Asset::LoadAsset(void* a_AssetDiskJobInfo)
{
	const AssetDiskJobInfo* a_JobInfo = reinterpret_cast<const AssetDiskJobInfo*>(a_AssetDiskJobInfo);

	AssetSlot t_AssetSlot{};
	BB_ASSERT(strlen(a_JobInfo->path) < _countof(t_AssetSlot.path) - 1, "Asset load path too long");
	strcpy(t_AssetSlot.path, a_JobInfo->path);

	t_AssetSlot.type = a_JobInfo->assetType;
	t_AssetSlot.hash.MakeHash(t_AssetSlot.path);

	switch (a_JobInfo->assetType)
	{
	case AssetType::IMAGE:
		switch (a_JobInfo->loadType)
		{
		case AssetLoadType::DISK:
			t_AssetSlot.image = LoadImage(t_AssetSlot.path);
			break;
		case AssetLoadType::MEMORY:
			BB_ASSERT(false, "Invalid AssetLoadType");
			break;
		default:
			BB_ASSERT(false, "Invalid AssetLoadType");
			break;
		}

		break;
	}

	s_AssetManager.assetMap.emplace(t_AssetSlot.hash, t_AssetSlot);
	return AssetHandle(t_AssetSlot.hash);
}

const RImageHandle Asset::GetImage(const AssetHandle a_Asset)
{
	const AssetSlot* t_Asset = s_AssetManager.assetMap.find(a_Asset.handle);
	BB_ASSERT(t_Asset->type == AssetType::IMAGE, "Asset found is not an image!");
	return t_Asset->image;
}

const RImageHandle Asset::GetImageWait(const char* a_Path)
{
	Hash t_Hash;
	t_Hash.MakeHash(a_Path);

	AssetSlot* t_Slot = s_AssetManager.assetMap.find(t_Hash);

	if (t_Slot != BB_INVALID_HANDLE)
		return t_Slot->image;

	AssetDiskJobInfo a_JobInfo{};
	a_JobInfo.assetType = AssetType::IMAGE;
	a_JobInfo.loadType = AssetLoadType::DISK;
	a_JobInfo.path = a_Path;

	LoadAsset(&a_JobInfo);
	t_Slot = s_AssetManager.assetMap.find(t_Hash);

	BB_ASSERT(t_Slot != BB_INVALID_HANDLE, "Uploaded a resource but still can't find it");

	return t_Slot->image;
}