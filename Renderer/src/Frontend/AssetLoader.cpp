#include "AssetLoader.hpp"
#include "RenderFrontend.h"


#pragma warning(push, 0)
#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"
#pragma warning (pop)

using namespace BB;

AssetLoader::AssetLoader(const AssetLoaderInfo& a_Info)
{
	{
		RenderCommandAllocatorCreateInfo t_CreateInfo;
		t_CreateInfo.name = "Async loading command allocator";
		t_CreateInfo.queueType = RENDER_QUEUE_TYPE::TRANSFER;
		t_CreateInfo.commandListCount = 1;
		m_CmdAllocator = RenderBackend::CreateCommandAllocator(t_CreateInfo);
	}
	{
		RenderCommandListCreateInfo t_CreateInfo;
		t_CreateInfo.name = "Async loading command list";
		t_CreateInfo.commandAllocator = m_CmdAllocator;
		m_CommandList = RenderBackend::CreateCommandList(t_CreateInfo);
	}

	RecordingCommandListHandle t_List = RenderBackend::StartCommandList(m_CommandList);

	switch (switch_on)
	{
	default:
		break;
	}
}

AssetLoader::~AssetLoader()
{
	RenderBackend::DestroyCommandAllocator(m_CmdAllocator);
	RenderBackend::DestroyCommandList(m_CommandList);
	m_Allocator.Clear();
}

bool AssetLoader::IsFinished()
{
	RenderWaitCommandsInfo t_WaitInfo;
}

void AssetLoader::LoadTexture(const AssetLoaderInfo& a_Info, RecordingCommandListHandle a_List)
{
	int x, y, c;
	//hacky way, whatever we do it for now.
	stbi_uc* t_Pixels = stbi_load(a_Info.path, &x, &y, &c, 4);
	RImageHandle t_NewImage;
	{
		RenderImageCreateInfo t_ImageInfo;
		t_ImageInfo.name = a_Info.path;
		t_ImageInfo.width = static_cast<uint32_t>(x);
		t_ImageInfo.height = static_cast<uint32_t>(y);
		t_ImageInfo.depth = 1;
		t_ImageInfo.arrayLayers = 1;
		t_ImageInfo.mipLevels = 1;
		t_ImageInfo.tiling = RENDER_IMAGE_TILING::OPTIMAL;
		t_ImageInfo.type = RENDER_IMAGE_TYPE::TYPE_2D;
		t_ImageInfo.format = RENDER_IMAGE_FORMAT::RGBA8_SRGB;
		t_NewImage = RenderBackend::CreateImage(t_ImageInfo);

		//Transfer image to prepare for transfer
		RenderTransitionImageInfo t_ImageTransInfo{};
		t_ImageTransInfo.srcMask = RENDER_ACCESS_MASK::NONE;
		t_ImageTransInfo.dstMask = RENDER_ACCESS_MASK::TRANSFER_WRITE;
		t_ImageTransInfo.image = t_NewImage;
		t_ImageTransInfo.oldLayout = RENDER_IMAGE_LAYOUT::UNDEFINED;
		t_ImageTransInfo.newLayout = RENDER_IMAGE_LAYOUT::TRANSFER_DST;
		t_ImageTransInfo.layerCount = 1;
		t_ImageTransInfo.levelCount = 1;
		t_ImageTransInfo.baseArrayLayer = 0;
		t_ImageTransInfo.baseMipLevel = 0;
		t_ImageTransInfo.srcStage = RENDER_PIPELINE_STAGE::TOP_OF_PIPELINE;
		t_ImageTransInfo.dstStage = RENDER_PIPELINE_STAGE::TRANSFER;
		RenderBackend::TransitionImage(a_List, t_ImageTransInfo);
	}


	const ImageReturnInfo t_ImageInfo = RenderBackend::GetImageInfo(t_NewImage);
	constexpr size_t TEXTURE_BYTE_ALIGNMENT = 512;
	//NEED TO DO ALIGNMENT ON THE IMAGE IF 
	UploadBuffer t_ImageUpload(t_ImageInfo.allocInfo.imageAllocByteSize);
	{
		const UINT64 t_SourcePitch = static_cast<UINT64>(t_ImageInfo.width) * sizeof(uint32_t);
		
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
		t_CopyImageInfo.dstImage = t_NewImage;
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

		RenderBackend::CopyBufferImage(a_List, t_CopyImageInfo);
	}
	RenderBackend::EndCommandList(a_List);
}

void AssetLoader::ExecuteCommands(RecordingCommandListHandle a_List)
{
	RenderQueue& t_TransferQueue = Render::GetTransferQueue();
	m_WaitValue = t_TransferQueue.GetNextFenceValue();

	ExecuteCommandsInfo a_ExecuteInfo{};
	a_ExecuteInfo.commands = &m_CommandList;
	a_ExecuteInfo.commandCount = 1;

	a_ExecuteInfo.signalFences = &t_TransferQueue.GetFence();
	a_ExecuteInfo.signalValues = &m_WaitValue;
	a_ExecuteInfo.signalCount = 1;

	t_TransferQueue.ExecuteCommands(&a_ExecuteInfo, 1);

	RenderWaitCommandsInfo t_WaitInfo;
	t_WaitInfo.waitFences = &t_TransferQueue.GetFence();
	t_WaitInfo.waitValues = &m_WaitValue;
	t_WaitInfo.waitCount = 1;
	RenderBackend::WaitCommands(t_WaitInfo);

	m_IsFinished = true;
}