#pragma once
#include "Slice.h"

namespace BB
{
	extern bool g_ShowEditor;

	class Editor
	{
	public:
		static void StartEditorFrame(const char* a_Name = "BB Engine Overview");
		static void EndEditorFrame();
		static void DisplayTextureManager();
		static void DisplaySceneInfo(class SceneGraph& t_Scene);
		static void DisplayRenderResources(class RenderResourceTracker& a_ResTracker);
		static void DisplayAllocator(BB::allocators::BaseAllocator& a_Allocator);
	};
}