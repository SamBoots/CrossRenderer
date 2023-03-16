#include "Camera.h"
#include "Math.inl"

using namespace BB;

Camera::Camera(const glm::vec3 a_Pos, const glm::vec3 a_Up, const glm::vec3 a_Right, const float a_CamSpeed)
{
	m_Pos = a_Pos;
	m_Right = a_Right;
	m_Speed = a_CamSpeed;

	m_Yaw = -90.0f;
	m_Pitch = 0.f;
	//we do not do anything yet.
	m_Orientation = glm::identity<glm::quat>();

	//Set the front.
	glm::quat rotation_yaw = glm::quat(glm::radians(m_Yaw), glm::vec3(0, 1, 0));
	glm::quat rotation_pitch = glm::quat(glm::radians(m_Pitch), glm::vec3(0, 1, 0));
	glm::quat rotation = rotation_yaw * rotation_pitch;

	glm::vec3 t_Forward = glm::normalize(m_Front);
	m_Right = glm::normalize(glm::cross(a_Up, t_Forward));
	m_Up = glm::normalize(glm::cross(t_Forward, m_Right));
	m_Front = rotation * glm::vec3(0, 0, -1);
}

Camera::~Camera()
{

}

void Camera::Move(const glm::vec3 a_Movement)
{
	glm::vec3 t_Forward = glm::normalize(m_Front);
	m_Right = glm::normalize(glm::cross(m_Up, t_Forward));
	m_Up = glm::normalize(glm::cross(t_Forward, m_Right));

	glm::vec3 t_Velocity{};
	t_Velocity *= t_Forward * a_Movement.z;
	t_Velocity *= m_Right * a_Movement.x;
	t_Velocity *= m_Up * a_Movement.y;

	t_Velocity *= m_Speed;

	m_Pos += t_Velocity;
}

void Camera::Rotate(const float a_Yaw, const float a_Pitch)
{
	m_Yaw += a_Yaw * m_Speed;
	m_Pitch += a_Pitch * m_Speed;

	m_Pitch = Clampf(m_Pitch, -90.f, 90.f);

	glm::quat rotation_pitch = glm::quat(glm::radians(m_Pitch), glm::vec3(1, 0, 0));
	glm::quat rotation_yaw = glm::quat(glm::radians(m_Yaw), glm::vec3(0, 1, 0));
	m_Orientation = rotation_pitch * rotation_yaw;
}

void Camera::SetSpeed(const float a_SpeedModifier)
{
	m_Speed = a_SpeedModifier;
}

const glm::mat4 Camera::CalculateView()
{
	m_Orientation = glm::normalize(m_Orientation);
	glm::mat4 t_Rotation = glm::mat4_cast(m_Orientation);

	glm::mat4 t_Translate = glm::mat4(1.0f);
	t_Translate = glm::translate(t_Translate, m_Pos);

	return t_Rotation * t_Translate;
}