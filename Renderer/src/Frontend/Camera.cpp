#include "Camera.h"
#include "Math.inl"

using namespace BB;

constexpr float3 UP_VECTOR = float3{ 0, 1.f, 0 };
constexpr float3 STANDARD_CAM_FRONT = float3{ 0, 0, -1 };

Camera::Camera(const float3 a_Pos, const float a_CamSpeed)
{
	m_Pos = a_Pos;
	m_Speed = a_CamSpeed;

	float3 t_Direction = Float3Normalize(m_Pos);

	m_Right = Float3Normalize(Float3Cross(UP_VECTOR, t_Direction));
	m_Up = Float3Cross(t_Direction, m_Right);
	m_Forward = STANDARD_CAM_FRONT;

	m_Yaw = 90.f;
	m_Pitch = 0;
}

Camera::~Camera()
{

}

void Camera::Move(const float3 a_Movement)
{
	float3 t_Velocity{0, 0, 0};
	if (a_Movement.z != 0)
		t_Velocity = t_Velocity + m_Forward * a_Movement.z;
	if (a_Movement.x != 0)
		t_Velocity = t_Velocity + Float3Normalize(Float3Cross(m_Forward, m_Up)) * a_Movement.x; //glm::normalize(glm::cross(m_Forward, m_Up)) * a_Movement.x;
	if (a_Movement.y != 0)
		t_Velocity = t_Velocity + UP_VECTOR * a_Movement.y;

	t_Velocity = t_Velocity * m_Speed;

	m_Pos = m_Pos + t_Velocity;
}

void Camera::Rotate(const float a_Yaw, const float a_Pitch)
{
	m_Yaw += a_Yaw * m_Speed;
	m_Pitch += a_Pitch * m_Speed;
	m_Pitch = Clampf(m_Pitch, -90.f, 90.f);

	float3 t_Direction{};
	t_Direction.x = cosf(m_Yaw) * cosf(m_Pitch);//cos(glm::radians(m_Yaw)) * cos(glm::radians(m_Pitch));
	t_Direction.y = sinf(m_Pitch);				//sin(glm::radians(m_Pitch));
	t_Direction.z = sinf(m_Yaw) * cosf(m_Pitch);//sin(glm::radians(m_Yaw)) * cos(glm::radians(m_Pitch));

	m_Forward = Float3Normalize(t_Direction); //glm::normalize(t_Direction);
}

void Camera::SetSpeed(const float a_SpeedModifier)
{
	m_Speed = a_SpeedModifier;
}

const Mat4x4 Camera::CalculateView()
{
	return Mat4x4Lookat(m_Pos, m_Pos + m_Forward, m_Up);
}