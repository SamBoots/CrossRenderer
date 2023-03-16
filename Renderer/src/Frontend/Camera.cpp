#include "Camera.h"
#include "Math.inl"

using namespace BB;

constexpr glm::vec3 STANDARD_CAM_UP = glm::vec3(0, 1.f, 0);
constexpr glm::vec3 STANDARD_CAM_FRONT = glm::vec3(0, 0, -1);

Camera::Camera(const glm::vec3 a_Pos, const float a_CamSpeed)
{
	m_Pos = a_Pos;
	m_Speed = a_CamSpeed;

	m_Yaw = 0;
	m_Pitch = 0;

	Rotate(0, 0);
}

Camera::~Camera()
{

}

void Camera::Move(const glm::vec3 a_Movement)
{
	glm::vec3 t_Velocity{0.0f};
	if (a_Movement.z != 0)
		t_Velocity += m_Front * a_Movement.z;
	if (a_Movement.x != 0)
		t_Velocity += m_Right * a_Movement.x;
	if (a_Movement.y != 0)
		t_Velocity += m_Up * a_Movement.y;

	t_Velocity *= m_Speed;

	m_Pos += t_Velocity;
}

void Camera::Rotate(const float a_Yaw, const float a_Pitch)
{
	m_Yaw += a_Yaw * m_Speed;
	m_Pitch += a_Pitch * m_Speed;
	m_Pitch = Clampf(m_Pitch, -85.f, 85.f);

	glm::quat rotation_pitch = glm::quat(glm::radians(m_Pitch), glm::vec3(1, 0, 0));
	glm::quat rotation_yaw = glm::quat(glm::radians(m_Yaw), glm::vec3(0, 1, 0));
	glm::quat orientation = rotation_pitch * rotation_yaw;

	{ //Rotate the front.
		glm::vec3 t_TempFront = STANDARD_CAM_FRONT;
		glm::quat t_ForQ = glm::quat(0.f, t_TempFront);

		glm::quat t_RotatedFront = orientation * t_ForQ;
		t_RotatedFront = t_RotatedFront * glm::conjugate(orientation);

		t_TempFront = glm::vec3(t_RotatedFront.x, t_RotatedFront.y, t_RotatedFront.z);
		m_Front = t_TempFront;
	}

	glm::vec3 t_NFront = glm::normalize(m_Front);
	m_Right = glm::normalize(glm::cross(STANDARD_CAM_UP, t_NFront));
	m_Up = glm::normalize(glm::cross(t_NFront, m_Right));
}

void Camera::SetSpeed(const float a_SpeedModifier)
{
	m_Speed = a_SpeedModifier;
}

const glm::mat4 Camera::CalculateView()
{
	return glm::lookAt(m_Pos, m_Pos + m_Front, m_Up);
}