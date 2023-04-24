#include "Editor.h"
#include "Frontend/RenderFrontend.h"
#include "Transform.h"

#include <glm/gtc/type_ptr.hpp>

#include "imgui.h"
using namespace BB;

void BB::Editor::DisplayDrawObjects(const BB::Slice<DrawObject> a_DrawObjects, const TransformPool& a_Pool)
{
	ImGui::Begin("Draw Objects");
	for (size_t i = 0; i < a_DrawObjects.size(); i++)
	{
		Transform& t_Trans = a_Pool.GetTransform(a_DrawObjects[i].transformHandle);
		
		ImGui::PushID(i);
		if (ImGui::CollapsingHeader("DrawObject"))
		{
			ImGui::SliderFloat3("Position", glm::value_ptr(t_Trans.m_Pos), -100, 100);
			ImGui::SliderFloat4("Rotation quat", glm::value_ptr(t_Trans.m_Rot), -100, 100);
			ImGui::SliderFloat3("Scale", glm::value_ptr(t_Trans.m_Scale), -100, 100);
		}
		ImGui::PopID();
	}
	ImGui::End();
}