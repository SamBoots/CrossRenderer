#include "Transform.h"
#include "glm/gtc/matrix_transform.hpp"
#include "RenderFrontend.h"

#include "Storage/Array.h"
#include "Storage/Slotmap.h"

using namespace BB;

Transform::Transform(const glm::vec3 a_Position)
	: Transform(a_Position, glm::vec3(0), 0, glm::vec3(1)) {}

Transform::Transform(const glm::vec3 a_Position, const glm::vec3 a_Axis, const float a_Radians)
	: Transform(a_Position, a_Axis, a_Radians, glm::vec3(1)) {}

Transform::Transform(const glm::vec3 a_Position, const glm::vec3 a_Axis, const float a_Radians, const glm::vec3 a_Scale)
	: m_Pos(a_Position), m_Scale(a_Scale) 
{
	m_State = TRANSFORM_STATE::REBUILD_MATRIX;
	m_Rot = glm::angleAxis(glm::radians(a_Radians), a_Axis);
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

struct BB::TransformPool_inst
{
	TransformPool_inst(Allocator a_SysAllocator, const uint32_t a_MatrixSize)
		:	systemAllocator(a_SysAllocator),
			poolIndices(a_SysAllocator, a_MatrixSize),
			pool(a_SysAllocator, a_MatrixSize),
			uploadMatrixBuffer(a_MatrixSize * sizeof(ModelBufferInfo))
	{};

	Allocator systemAllocator;

	Array<uint32_t> poolIndices;
	Slotmap<Transform> pool;
	UploadBuffer uploadMatrixBuffer;
};

TransformPool::TransformPool(Allocator a_SysAllocator, const uint32_t a_MatrixSize)
{
	inst = BBnew(a_SysAllocator, TransformPool_inst)(a_SysAllocator, a_MatrixSize);
}

TransformPool::~TransformPool()
{
	Allocator t_Allocator = inst->systemAllocator;
	BBfree(t_Allocator, inst);
}

TransformHandle TransformPool::CreateTransform(const glm::vec3 a_Position)
{
	TransformHandle t_Handle(inst->pool.emplace(a_Position).handle);
	inst->poolIndices.emplace(t_Handle.index);
	return t_Handle;
}

TransformHandle TransformPool::CreateTransform(const glm::vec3 a_Position, const glm::vec3 a_Axis, const float a_Radians)
{
	TransformHandle t_Handle(inst->pool.emplace(a_Position, a_Axis, a_Radians).handle);
	inst->poolIndices.emplace(t_Handle.index);
	return t_Handle;
}

TransformHandle TransformPool::CreateTransform(const glm::vec3 a_Position, const glm::vec3 a_Axis, const float a_Radians, const glm::vec3 a_Scale)
{
	TransformHandle t_Handle(inst->pool.emplace(a_Position, a_Axis, a_Radians, a_Scale).handle);
	inst->poolIndices.emplace_back(t_Handle.index);
	return t_Handle;
}

void TransformPool::FreeTransform(const TransformHandle a_Handle)
{
	//erase element from array.
	inst->poolIndices;
	inst->pool.erase(a_Handle.handle);
}

Transform& TransformPool::GetTransform(const TransformHandle a_Handle) const
{
	return inst->pool[a_Handle.handle];
}

void TransformPool::UpdateTransforms()
{
	void* t_GPUBufferStart = inst->uploadMatrixBuffer.GetStart();

	for (size_t i = 0; i < inst->poolIndices.size(); i++)
	{
		const uint32_t t_Index = inst->poolIndices[i];
		//if (inst->pool[t_Index].GetState() == TRANSFORM_STATE::REBUILD_MATRIX)
		{
			ModelBufferInfo t_Pack{};
			t_Pack.model = inst->pool[t_Index].CreateModelMatrix();
			t_Pack.inverseModel = glm::inverse(t_Pack.model);

			//Copy the model matrix into the transfer buffer.
			memcpy(Pointer::Add(t_GPUBufferStart, t_Index * sizeof(ModelBufferInfo)),
				&t_Pack,
				sizeof(ModelBufferInfo));
		}
	}
}

const uint32_t TransformPool::PoolSize() const
{
	return inst->pool.capacity();
}

const UploadBuffer& TransformPool::PoolGPUUploadBuffer()
{
	return inst->uploadMatrixBuffer;
}