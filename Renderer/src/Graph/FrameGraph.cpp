#include "FrameGraph.h"

#include "Slotmap.h"

using namespace BB;

struct FrameGraphNode
{
	const char* name = nullptr;


};

struct BB::FrameGraph_inst
{
	FrameGraph_inst(Allocator a_Allocator) :
		nodes(a_Allocator, 128), resources(a_Allocator, 256) {};

	Slotmap<FrameGraphNode> nodes;
	Slotmap<FrameGraphResource> resources;
};

FrameGraph::FrameGraph()
{
	m_Inst = BBnew(m_Allocator, FrameGraph_inst)(m_Allocator);
}

FrameGraph::~FrameGraph()
{
	BBfree(m_Allocator, m_Inst);
}

const FrameGraphResourceHandle FrameGraph::CreateResource(const FrameGraphResource& a_Resource)
{
	return FrameGraphResourceHandle(m_Inst->resources.emplace(a_Resource).handle);
}

void FrameGraph::DestroyResource(const FrameGraphResourceHandle a_Handle)
{
	m_Inst->resources.erase(a_Handle.handle);
}