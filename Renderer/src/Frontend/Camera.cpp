#include "Camera.h"
#include "Math.inl"

using namespace BB;

constexpr glm::vec3 UP_VECTOR = glm::vec3(0, 1.f, 0);
constexpr glm::vec3 STANDARD_CAM_FRONT = glm::vec3(0, 0, -1);

Camera::Camera(const glm::vec3 a_Pos, const float a_CamSpeed)
{
	m_Pos = a_Pos;
	m_Speed = a_CamSpeed;

	glm::vec3 t_Direction = glm::normalize(m_Pos);

	m_Right = glm::normalize(glm::cross(UP_VECTOR, t_Direction));
	m_Up = glm::cross(t_Direction, m_Right);
	m_Forward = STANDARD_CAM_FRONT;

	m_Yaw = 90.f;
	m_Pitch = 0;
}

Camera::~Camera()
{

}

void Camera::Move(const glm::vec3 a_Movement)
{
	glm::vec3 t_Velocity{0.0f};
	if (a_Movement.z != 0)
		t_Velocity += m_Forward * a_Movement.z;
	if (a_Movement.x != 0)
		t_Velocity += glm::normalize(glm::cross(m_Forward, m_Up)) * a_Movement.x;
	if (a_Movement.y != 0)
		t_Velocity += UP_VECTOR * a_Movement.y;

	t_Velocity *= m_Speed;

	m_Pos += t_Velocity;
}

void Camera::Rotate(const float a_Yaw, const float a_Pitch)
{
	m_Yaw += a_Yaw * m_Speed;
	m_Pitch += a_Pitch * m_Speed;
	m_Pitch = Clampf(m_Pitch, -90.f, 90.f);

	glm::vec3 t_Direction{};
	t_Direction.x = cos(glm::radians(m_Yaw)) * cos(glm::radians(m_Pitch));
	t_Direction.y = sin(glm::radians(m_Pitch));
	t_Direction.z = sin(glm::radians(m_Yaw)) * cos(glm::radians(m_Pitch));

	m_Forward = glm::normalize(t_Direction);
}

void Camera::SetSpeed(const float a_SpeedModifier)
{
	m_Speed = a_SpeedModifier;
}

const glm::mat4 Camera::CalculateView()
{
	return glm::lookAt(m_Pos, m_Pos + m_Forward, m_Up);
}