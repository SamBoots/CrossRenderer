// dear imgui: Renderer Backend for CrossRenderer

#include "imgui.h"      // IMGUI_IMPL_API
#include "RenderBackendCommon.h"

struct ImGui_ImplCross_InitInfo
{
	uint32_t imageCount = 0;
	uint32_t minImageCount = 0;
	BB::ShaderCodeHandle vertexShader{};
	BB::ShaderCodeHandle fragmentShader{};
};

// Called by user code
IMGUI_IMPL_API bool ImGui_ImplCross_Init(const ImGui_ImplCross_InitInfo& a_Info);
IMGUI_IMPL_API void ImGui_ImplCross_Shutdown();
IMGUI_IMPL_API void ImGui_ImplCross_NewFrame();
IMGUI_IMPL_API void ImGui_ImplCross_RenderDrawData(const ImDrawData& a_DrawData, const BB::RecordingCommandListHandle a_CommandList, const BB::PipelineHandle a_Pipeline = nullptr);
IMGUI_IMPL_API bool ImGui_ImplCross_CreateFontsTexture(const BB::RecordingCommandListHandle a_CommandList);
IMGUI_IMPL_API void ImGui_ImplCross_DestroyFontUploadObjects();
IMGUI_IMPL_API void ImGui_ImplCross_SetMinImageCount(const uint32_t min_image_count); // To override MinImageCount after initialization (e.g. if swap chain is recreated)