#pragma once
#include "RenderFrontendCommon.h"

namespace BB
{
	class Camera
	{
	public:
		Camera(const glm::vec3 a_Pos, const glm::vec3 a_Up, const glm::vec3 a_Right, const float a_CamSpeed = 0.15f);
		~Camera();

		void Move(const glm::vec3 a_Movement);
		void Rotate(const float a_Yaw, const float a_Pitch);
		void SetSpeed(const float a_SpeedModifier);

		const glm::mat4 CalculateView();
	private:
		float m_Yaw;
		float m_Pitch;
		float m_Speed;

		glm::vec3 m_Pos;
		glm::vec3 m_Front;
		glm::vec3 m_Right;
		glm::vec3 m_Up;	
		glm::quat m_Orientation;
	};
}