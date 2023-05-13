#include "Editor.h"
#include "Frontend/RenderFrontend.h"
#include "Transform.h"
#include "LightSystem.h"

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

void BB::Editor::DisplayLightSystem(const BB::LightSystem& a_System)
{
	//ImGui::Begin("Light Pool");

	//if (ImGui::CollapsingHeader(a_PoolName))
	//{
	//	ImGui::Text("Light amount: %u/%u", a_Pool.GetLightCount(), a_Pool.GetLightMax());

	//	const Slice<Light> t_Lights = a_Pool.GetLights();

	//	for (size_t i = 0; i < t_Lights.size(); i++)
	//	{
	//		if (ImGui::TreeNode((void*)(intptr_t)i, "Light %d", i))
	//		{
	//			ImGui::SliderFloat3("Position", &t_Lights[i].pos.x, -100, 100);
	//			ImGui::SliderFloat("Radius", &t_Lights[i].radius, 0, 10);
	//			ImGui::SliderFloat4("Color", &t_Lights[i].color.x, 0, 255);
	//		}
	//	}
	//}

	//ImGui::End();
}