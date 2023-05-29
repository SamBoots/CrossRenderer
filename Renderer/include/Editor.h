#pragma once
#include "Slice.h"

namespace BB
{
	struct DrawObject;
	struct LightSystem;
	class RenderResourceTracker;
	class TransformPool;

	class Editor
	{
	public:
		static void DisplayDrawObjects(const BB::Slice<DrawObject> a_DrawObjects, const TransformPool& a_Pool);
		static void DisplayLightSystem(const BB::LightSystem& a_System);
		static void DisplayRenderResources(BB::RenderResourceTracker& a_ResTracker);
	};
}