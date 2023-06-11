#include "SceneGraph.hpp"
#include "Array.h"

#include "glm/glm.hpp"

using namespace BB;

struct SceneGraph_inst
{
	SceneGraph_inst(Allocator a_Allocator)
		: allocator(a_Allocator), matrices(a_Allocator, 128) {};

	Allocator allocator;
	Array<glm::mat4> matrices;
};

SceneGraph::SceneGraph(Allocator a_Allocator)
{
	inst = BBnew(a_Allocator, SceneGraph_inst)(a_Allocator);
}

SceneGraph::~SceneGraph()
{
	Allocator t_Allocator = inst->allocator;
	BBfree(t_Allocator, inst);
}