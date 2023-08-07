#include "Transform.h"
#include "glm/gtc/matrix_transform.hpp"
#include "RenderFrontend.h"

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

//slotmap type of data structure.
struct TransformNode
{
	union //44 bytes
	{
		Transform transform; 
		uint32_t next;
	};
	
	uint32_t generation; //48 bytes
};

struct BB::TransformPool_inst
{
	TransformPool_inst(Allocator a_SysAllocator, const uint32_t a_TransformCount)
		:	systemAllocator(a_SysAllocator),
			uploadMatrixBuffer(a_TransformCount * sizeof(ModelBufferInfo))
	{
		transformCount = a_TransformCount;
		transforms = reinterpret_cast<TransformNode*>(BBalloc(a_SysAllocator, sizeof(TransformNode) * a_TransformCount));

		for (size_t i = 0; i < static_cast<size_t>(transformCount - 1); i++)
		{
			transforms[i].next = i + 1;
			transforms[i].generation = 1;
		}

		transforms[transformCount - 1].next = UINT32_MAX;
		transforms[transformCount - 1].generation = 1;
	};

	Allocator systemAllocator;
	UploadBuffer uploadMatrixBuffer;

	uint32_t transformCount;
	uint32_t nextFreeTransform;

	TransformNode* transforms;
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
	const uint32_t t_TransformIndex = inst->nextFreeTransform;
	TransformNode* t_Node = &inst->transforms[t_TransformIndex];
	inst->nextFreeTransform = t_Node->next;

	//WILL OVERWRITE t_Node->next due to it being a union.
	new (&t_Node->transform) Transform(a_Position);

	return TransformHandle(t_TransformIndex, t_Node->generation);
}

TransformHandle TransformPool::CreateTransform(const glm::vec3 a_Position, const glm::vec3 a_Axis, const float a_Radians)
{
	const uint32_t t_TransformIndex = inst->nextFreeTransform;
	TransformNode* t_Node = &inst->transforms[t_TransformIndex];
	inst->nextFreeTransform = t_Node->next;

	//WILL OVERWRITE t_Node->next due to it being a union.
	new (&t_Node->transform) Transform(a_Position, a_Axis, a_Radians);

	return TransformHandle(t_TransformIndex, t_Node->generation);
}

TransformHandle TransformPool::CreateTransform(const glm::vec3 a_Position, const glm::vec3 a_Axis, const float a_Radians, const glm::vec3 a_Scale)
{
	const uint32_t t_TransformIndex = inst->nextFreeTransform;
	TransformNode* t_Node = &inst->transforms[t_TransformIndex];
	inst->nextFreeTransform = t_Node->next;

	//WILL OVERWRITE t_Node->next due to it being a union.
	new (&t_Node->transform) Transform(a_Position, a_Axis, a_Radians, a_Scale);

	return TransformHandle(t_TransformIndex, t_Node->generation);
}

void TransformPool::FreeTransform(const TransformHandle a_Handle)
{
	BB_ASSERT(a_Handle.extraIndex == inst->transforms[a_Handle.index].generation, "Transform likely freed twice.")

	//mark transform as free.
	inst->transforms[a_Handle.index].next = inst->transforms->next;
	++inst->transforms[a_Handle.index].generation;
	inst->transforms->next = a_Handle.index;
}

Transform& TransformPool::GetTransform(const TransformHandle a_Handle) const
{
	BB_ASSERT(a_Handle.extraIndex == inst->transforms[a_Handle.index].generation, "Transform likely freed twice.")
	return inst->transforms[a_Handle.index].transform;
}

void TransformPool::UpdateTransforms()
{
	void* t_GPUBufferStart = inst->uploadMatrixBuffer.GetStart();

	for (size_t i = 0; i < static_cast<size_t>(inst->transformCount); i++)
	{
		//if (inst->pool[t_Index].GetState() == TRANSFORM_STATE::REBUILD_MATRIX)
		{
			ModelBufferInfo t_Pack{};
			t_Pack.model = inst->transforms[i].transform.CreateModelMatrix();
			t_Pack.inverseModel = glm::inverse(t_Pack.model);

			//Copy the model matrix into the transfer buffer.
			memcpy(Pointer::Add(t_GPUBufferStart, i * sizeof(ModelBufferInfo)),
				&t_Pack,
				sizeof(ModelBufferInfo));
		}
	}
}

const uint32_t TransformPool::PoolSize() const
{
	return inst->transformCount;
}

const UploadBuffer& TransformPool::PoolGPUUploadBuffer()
{
	return inst->uploadMatrixBuffer;
}