#include "Transform.h"
#include "glm/gtc/matrix_transform.hpp"

using namespace BB;

Transform::Transform(const glm::vec3 a_Position)
	: Transform(a_Position, glm::vec3(0), 0, glm::vec3(1)) {}

Transform::Transform(const glm::vec3 a_Position, const glm::vec3 a_Axis, const float a_Radians)
	: Transform(a_Position, a_Axis, a_Radians, glm::vec3(1)) {}

Transform::Transform(const glm::vec3 a_Position, const glm::vec3 a_Axis, const float a_Radians, const glm::vec3 a_Scale)
	: m_Pos(a_Position), m_Scale(a_Scale) 
{
	m_Rot = glm::angleAxis(a_Radians, a_Axis);
}

void Transform::Translate(const glm::vec3 a_Translation)
{
	m_State = TRANSFORM_STATE::REBUILD_MATRIX;
	m_Pos += a_Translation;
}

void Transform::Rotate(const glm::vec3 a_Axis, const float a_Radians)
{
	m_State = TRANSFORM_STATE::REBUILD_MATRIX;
	m_Rot = glm::rotate(m_Rot, a_Radians, a_Axis);
}

void Transform::SetPosition(const glm::vec3 a_Position)
{
	m_State = TRANSFORM_STATE::REBUILD_MATRIX;
	m_Pos = a_Position;
}

void Transform::SetRotation(const glm::vec3 a_Axis, const float a_Radians)
{
	m_State = TRANSFORM_STATE::REBUILD_MATRIX;
	m_Rot = glm::angleAxis(a_Radians, a_Axis);
}

void Transform::SetScale(const glm::vec3 a_Scale)
{
	m_State = TRANSFORM_STATE::REBUILD_MATRIX;
	m_Scale = a_Scale;
}

glm::mat4 Transform::CreateModelMatrix()
{
	glm::mat4 t_Matrix = glm::mat4(1.0f);
	t_Matrix = glm::translate(t_Matrix, m_Pos);
	t_Matrix *= glm::mat4_cast(m_Rot);
	t_Matrix = glm::scale(t_Matrix, m_Scale);
	m_State = TRANSFORM_STATE::NO_ACTION;
	return t_Matrix;
}



TransformPool::TransformPool(Allocator a_SysAllocator, void* a_GPUMemoryRegion, const uint32_t a_MatrixSize)
	:	m_Pool(a_SysAllocator, a_MatrixSize), m_TransferBufferRegions(a_SysAllocator, a_MatrixSize)
{
	//Set all the memory regions.
	glm::mat4* t_MemRegion = reinterpret_cast<glm::mat4*>(a_GPUMemoryRegion);
	for (uint32_t i = 0; i < a_MatrixSize; i++)
	{
		m_TransferBufferRegions.emplace_back(t_MemRegion++);
	}
}

Transform& TransformPool::GetTransform()
{
	m_Pool.emplace_back(glm::vec3(0));
	return m_Pool[m_Pool.size() - 1];
}

void TransformPool::UpdateTransforms()
{
	for (size_t i = 0; i < m_Pool.size(); i++)
	{
		if (m_Pool[i].GetState() == TRANSFORM_STATE::REBUILD_MATRIX)
		{
			//Copy the model matrix into the transfer buffer.
			memcpy(m_TransferBufferRegions[i], &m_Pool[i].CreateModelMatrix(), sizeof(glm::mat4));

		}
	}
}