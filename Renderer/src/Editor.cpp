#include "Editor.h"
#include "Frontend/RenderFrontend.h"

#include "Transform.h"
#include "LightSystem.h"
#include "RenderResourceTracker.h"
#include "BBMemory.h"

#include <glm/gtc/type_ptr.hpp>

#include "imgui.h"
using namespace BB;

bool BB::g_ShowEditor;

void BB::Editor::StartEditorFrame(const char* a_Name)
{
	g_ShowEditor = ImGui::Begin(a_Name);
}

void BB::Editor::EndEditorFrame()
{
	ImGui::End();
}

void BB::Editor::DisplayDrawObjects(const BB::Slice<DrawObject> a_DrawObjects, const TransformPool& a_Pool)
{
	if (!g_ShowEditor)
		return;

	if (ImGui::CollapsingHeader("Draw Objects"))
	{
		
		for (size_t i = 0; i < a_DrawObjects.size(); i++)
		{
			Transform& t_Trans = a_Pool.GetTransform(a_DrawObjects[i].transformHandle);

			ImGui::PushID(static_cast<int>(i));
			ImGui::Indent();
			if (ImGui::CollapsingHeader("DrawObject"))
			{
				ImGui::SliderFloat3("Position", glm::value_ptr(t_Trans.m_Pos), -100, 100);
				ImGui::SliderFloat4("Rotation quat", glm::value_ptr(t_Trans.m_Rot), -100, 100);
				ImGui::SliderFloat3("Scale", glm::value_ptr(t_Trans.m_Scale), -100, 100);

			}
			ImGui::Unindent();
			ImGui::PopID();
		}
	}
}

void BB::Editor::DisplayAllocator(BB::allocators::BaseAllocator& a_Allocator)
{
	if (!g_ShowEditor)
		return;

	if (ImGui::CollapsingHeader(a_Allocator.name))
	{
		ImGui::Text("Allocation Logs");
		allocators::BaseAllocator::AllocationLog* t_Log = a_Allocator.frontLog;
		int i = 0;
		while (t_Log != nullptr)
		{
			if (ImGui::TreeNode((void*)(intptr_t)i++, t_Log->file))
			{
				ImGui::Text("Size: %u", t_Log->allocSize);
				ImGui::Text("File: %s", t_Log->file);
				ImGui::Text("Line: %u", t_Log->line);
				ImGui::TreePop();
			}
			t_Log = t_Log->prev;
		}
	}
}