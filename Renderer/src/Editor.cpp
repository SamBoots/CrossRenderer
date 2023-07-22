#include "Editor.h"
#include "Frontend/RenderFrontend.h"

#include "Transform.h"
#include "LightSystem.h"
#include "RenderResourceTracker.h"
#include "BBMemory.h"

#include "SceneGraph.hpp"

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
	ImGui::Render();
}

void BB::Editor::DisplaySceneInfo(SceneGraph& t_Scene)
{
	if (!g_ShowEditor)
		return;

	if (ImGui::CollapsingHeader(t_Scene.GetSceneName()))
	{
		ImGui::Indent();
		if (ImGui::CollapsingHeader("Scene Objects"))
		{
			ImGui::Indent();
			BB::Slice<DrawObject> t_DrawObjects = t_Scene.GetDrawObjects();
			for (size_t i = 0; i < t_DrawObjects.size(); i++)
			{
				const DrawObject& t_Obj = t_DrawObjects[i];
				Transform& t_Trans = t_Scene.GetTransform(t_Obj.transformHandle);

				if (ImGui::CollapsingHeader(t_Obj.name))
				{
					ImGui::PushID(static_cast<int>(i));
					ImGui::Indent();
					if (ImGui::CollapsingHeader("Transform"))
					{
						ImGui::Indent();
						ImGui::InputFloat3("Position", glm::value_ptr(t_Trans.m_Pos));
						ImGui::InputFloat3("Rotation quat", glm::value_ptr(t_Trans.m_Rot));
						ImGui::InputFloat3("Scale", glm::value_ptr(t_Trans.m_Scale));
						ImGui::Unindent();
					}
					if (ImGui::CollapsingHeader("ModelInfo"))
					{
						ImGui::Indent();
						ImGui::Text("Do some model meta info here....");
						ImGui::Unindent();
					}

					ImGui::Unindent();
					ImGui::PopID();
				}
			}
			ImGui::Unindent();
		}
		ImGui::Unindent();
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