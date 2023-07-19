#include "Editor.h"
#include "Frontend/RenderFrontend.h"

#include "Transform.h"
#include "LightSystem.h"
#include "RenderResourceTracker.h"

#include "SceneGraph.hpp"

#include <glm/gtc/type_ptr.hpp>
#include "imgui.h"
using namespace BB;

void BB::Editor::DisplaySceneInfo(SceneGraph& t_Scene)
{
	if (ImGui::Begin(t_Scene.GetSceneName()))
	{
		if (ImGui::CollapsingHeader("Scene Objects"))
		{
			BB::Slice<DrawObject> t_DrawObjects = t_Scene.GetDrawObjects();
			ImGui::Indent();
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

		if (ImGui::CollapsingHeader("Lights"))
		{
			ImGui::Indent();
			Slice<Light> t_Lights = t_Scene.GetLights();
			for (size_t i = 0; i < t_Lights.size(); i++)
			{
				Light& t_Light = t_Lights[i];

				ImGui::PushID(static_cast<int>(i));
				if (ImGui::CollapsingHeader("Light"))
				{
					ImGui::Indent();
					ImGui::InputFloat3("Position", &t_Light.pos.x);
					ImGui::InputFloat4("Color", &t_Light.color.x);
					ImGui::InputFloat("Scale", &t_Light.radius);
					ImGui::Unindent();
				}
				ImGui::PopID();
			}
			ImGui::Unindent();
		}

		ImGui::End();
	}
}