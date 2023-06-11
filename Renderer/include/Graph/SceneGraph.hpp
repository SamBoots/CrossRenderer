#pragma once
#include "Common.h"

struct SceneGraph
{
	SceneGraph(Allocator a_Allocator);
	~SceneGraph();

	struct SceneGraph_inst* inst;
};