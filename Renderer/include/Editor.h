#pragma once
#include "Slice.h"

namespace BB
{
	struct DrawObject;
	class LightSystem;
	class RenderResourceTracker;
	class TransformPool;
	class allocators::BaseAllocator;

	extern bool g_ShowEditor;

	class Editor
	{
	public:
		static void StartEditorFrame(const char* a_Name = "BB Engine Overview");
		static void EndEditorFrame();
		static void DisplayDrawObjects(const BB::Slice<DrawObject> a_DrawObjects, const TransformPool& a_Pool);
		static void DisplayLightSystem(const BB::LightSystem& a_System);
		static void DisplayRenderResources(BB::RenderResourceTracker& a_ResTracker);
		static void DisplayAllocator(BB::allocators::BaseAllocator& a_Allocator);
	};
}