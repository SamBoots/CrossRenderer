#include "FrameGraph.hpp"

#include "Array.h"
#include "Slotmap.h"

#include "RenderFrontend.h"

using namespace BB;

struct FrameGraphNode
{
	const char* name = nullptr;
	FrameGraphRenderPass renderPass;
};

struct FrameData
{
	uint64_t graphicsFenceValue = 0;
	uint64_t transferFenceValue = 0;
};

struct BB::FrameGraph_inst
{
	FrameGraph_inst(Allocator a_Allocator) :
		renderpasses(a_Allocator, 8), nodes(a_Allocator, 128), resources(a_Allocator, 256) {};

	//TEMP
	CommandList* commandList = nullptr;

	Array<FrameGraphRenderPass> renderpasses;

	Slotmap<FrameGraphNode> nodes;
	Slotmap<FrameGraphResource> resources;

	FrameIndex currentFrame = 0;
	FrameData frameData[3]{};
};

FrameGraph::FrameGraph()
{
	inst = BBnew(m_Allocator, FrameGraph_inst)(m_Allocator);
}

FrameGraph::~FrameGraph()
{
	BBfree(m_Allocator, inst);
}

void FrameGraph::RegisterRenderPass(FrameGraphRenderPass a_RenderPass)
{
	inst->renderpasses.push_back(a_RenderPass);
}

//temporary for now.
void FrameGraph::BeginRendering()
{
	//wait for the previous frame to be completely done.
	Render::GetGraphicsQueue().WaitFenceValue(inst->frameData[inst->currentFrame].graphicsFenceValue);
	Render::GetTransferQueue().WaitFenceValue(inst->frameData[inst->currentFrame].transferFenceValue);

	inst->commandList = Render::GetGraphicsQueue().GetCommandList();
	RenderBackend::BindDescriptorHeaps(inst->commandList->list, Render::GetGPUHeap(inst->currentFrame), BB_INVALID_HANDLE);
	
	StartFrameInfo t_StartInfo{};
	RenderBackend::StartFrame(t_StartInfo);
	Render::StartFrame(inst->commandList->list);

	for (size_t i = 0; i < inst->renderpasses.size(); i++)
	{
		GraphPreRenderInfo t_Info{ inst->renderpasses[i].instance };
		inst->renderpasses[i].preRenderFunc(inst->commandList->list, t_Info);
	}
}

void FrameGraph::Render()
{
	Render::Update(0);
	Render::UploadDescriptorsToGPU(inst->currentFrame);
	for (size_t i = 0; i < inst->renderpasses.size(); i++)
	{
		GraphRenderInfo t_Info{ inst->renderpasses[i].instance };
		t_Info.currentLayout = RENDER_IMAGE_LAYOUT::UNDEFINED;
		t_Info.renderLayout = RENDER_IMAGE_LAYOUT::COLOR_ATTACHMENT_OPTIMAL;
		t_Info.endLayout = RENDER_IMAGE_LAYOUT::COLOR_ATTACHMENT_OPTIMAL;
		inst->renderpasses[i].renderFunc(inst->commandList->list, t_Info);
	}
}

void FrameGraph::EndRendering()
{
	for (size_t i = 0; i < inst->renderpasses.size(); i++)
	{
		GraphPostRenderInfo t_Info{ inst->renderpasses[i].instance };
		inst->renderpasses[i].postRenderFunc(inst->commandList->list, t_Info);
	}

	inst->frameData[inst->currentFrame].graphicsFenceValue = Render::GetGraphicsQueue().GetNextFenceValue() - 1;
	inst->frameData[inst->currentFrame].transferFenceValue = Render::GetTransferQueue().GetNextFenceValue() - 1;

	Render::EndFrame(inst->commandList->list);
	RenderBackend::EndCommandList(inst->commandList->list);
	Render::GetGraphicsQueue().ExecutePresentCommands(&inst->commandList, 1, nullptr, nullptr, 0);

	PresentFrameInfo t_PresentFrame{};
	inst->currentFrame = RenderBackend::PresentFrame(t_PresentFrame);
}

const FrameGraphResourceHandle FrameGraph::CreateResource(const FrameGraphResource& a_Resource)
{
	return FrameGraphResourceHandle(inst->resources.emplace(a_Resource).handle);
}

void FrameGraph::DestroyResource(const FrameGraphResourceHandle a_Handle)
{
	inst->resources.erase(a_Handle.handle);
}