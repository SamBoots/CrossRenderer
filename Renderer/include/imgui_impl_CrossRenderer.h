// dear imgui: Renderer Backend for CrossRenderer, referenced of the imgui backend base for Vulkan.

#include "imgui.h"      // IMGUI_IMPL_API
#include "RenderBackend.h"
#include "HID.h"

struct ImGui_ImplCross_InitInfo
{
	BB::WindowHandle window;
	uint32_t imageCount = 0;
	uint32_t minImageCount = 0;
	BB::ShaderCodeHandle vertexShader{};
	BB::ShaderCodeHandle fragmentShader{};
};

// Called by user code
IMGUI_IMPL_API bool ImGui_ImplCross_Init(const ImGui_ImplCross_InitInfo& a_Info);
IMGUI_IMPL_API void ImGui_ImplCross_Shutdown();
IMGUI_IMPL_API void ImGui_ImplCross_NewFrame();
IMGUI_IMPL_API void ImGui_ImplCross_RenderDrawData(const ImDrawData& a_DrawData, const BB::CommandListHandle a_CommandList, const BB::PipelineHandle a_Pipeline = BB::BB_INVALID_HANDLE);
IMGUI_IMPL_API bool ImGui_ImplCross_CreateFontsTexture(const BB::CommandListHandle a_CommandList, BB::UploadBuffer& a_UploadBuffer);
IMGUI_IMPL_API void ImGui_ImplCross_DestroyFontUploadObjects();

IMGUI_IMPL_API bool ImGui_ImplCross_ProcessInput(const BB::InputEvent& a_InputEvent);