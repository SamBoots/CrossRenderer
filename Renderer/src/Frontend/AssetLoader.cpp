#include "AssetLoader.hpp"
#include "RenderFrontend.h"

using namespace BB;

struct BB::AssetLoader_inst
{
	RFenceHandle fence;
	CommandAllocatorHandle cmdAllocator;
	CommandListHandle commandList;
};

AssetLoader::AssetLoader(const AssetLoaderInfo& a_Info)
{
	inst = BBnew(m_Allocator, AssetLoader_inst);
	{
		FenceCreateInfo t_CreateInfo;
		t_CreateInfo.name = "Async loading fence";
		inst->fence = RenderBackend::CreateFence(t_CreateInfo);
	}
	{
		RenderCommandAllocatorCreateInfo t_CreateInfo;
		t_CreateInfo.name = "Async loading command allocator";
		t_CreateInfo.queueType = RENDER_QUEUE_TYPE::TRANSFER_COPY;
		t_CreateInfo.commandListCount = 1;
		inst->cmdAllocator = RenderBackend::CreateCommandAllocator(t_CreateInfo);
	}
	{
		RenderCommandListCreateInfo t_CreateInfo;
		t_CreateInfo.name = "Async loading command list";
		t_CreateInfo.commandAllocator = inst->cmdAllocator;
		inst->commandList = RenderBackend::CreateCommandList(t_CreateInfo);
	}
}

AssetLoader::~AssetLoader()
{
	RenderBackend::DestroyFence(inst->fence);
	RenderBackend::DestroyCommandAllocator(inst->cmdAllocator);
	RenderBackend::DestroyCommandList(inst->commandList);
	m_Allocator.Clear();
}

bool AssetLoader::IsFinished()
{

}