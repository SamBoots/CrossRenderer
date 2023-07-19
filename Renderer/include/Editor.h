#pragma once
#include "Slice.h"

namespace BB
{
	class Editor
	{
	public:
		static void DisplaySceneInfo(struct SceneGraph& t_Scene);
		static void DisplayRenderResources(struct RenderResourceTracker& a_ResTracker);
	};
}