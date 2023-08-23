#include "Transform.h"
#include "RenderFrontend.h"
#include "Math.inl"

using namespace BB;

Transform::Transform(const float3 a_Position)
	: Transform(a_Position, float3{0,0,0}, 0, float3{1,1,1}) {}

Transform::Transform(const float3 a_Position, const float3 a_Axis, const float a_Radians)
	: Transform(a_Position, a_Axis, a_Radians, float3{1,1,1}) {}

Transform::Transform(const float3 a_Position, const float3 a_Axis, const float a_Radians, const float3 a_Scale)
	: m_Pos(a_Position), m_Scale(a_Scale) 
{
	m_Rot = QuatFromAxisAngle(a_Axis, a_Radians); //glm::angleAxis(glm::radians(a_Radians), a_Axis);
}

void Transform::Translate(const float3 a_Translation)
{
	m_Pos = m_Pos + a_Translation;
}

void Transform::Rotate(const float3 a_Axis, const float a_Radians)
{
	//m_Rot = m_Rot * Quat{ a_Axis.x,a_Axis.y,a_Axis.z, a_Radians }; //glm::rotate(m_Rot, a_Radians, a_Axis);
}

void Transform::SetPosition(const float3 a_Position)
{
	m_Pos = a_Position;
}

void Transform::SetRotation(const float3 a_Axis, const float a_Radians)
{
	m_Rot = QuatFromAxisAngle(a_Axis, a_Radians); //glm::angleAxis(a_Radians, a_Axis);
}

void Transform::SetScale(const float3 a_Scale)
{
	m_Scale = a_Scale;
}

const Mat4x4 Transform::CreateMatrix()
{
	Mat4x4 t_Matrix = Mat4x4Identity();
	t_Matrix = t_Matrix * Mat4x4FromTranslation(m_Pos);
	t_Matrix = t_Matrix * Mat4x4FromQuat(m_Rot);
	t_Matrix = Mat4x4Scale(t_Matrix, m_Scale);
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
		:	systemAllocator(a_SysAllocator)
	{
		transformCount = a_TransformCount;
		nextFreeTransform = 0;
		transforms = reinterpret_cast<TransformNode*>(BBalloc(a_SysAllocator, sizeof(TransformNode) * a_TransformCount));
		for (size_t i = 0; i < static_cast<size_t>(transformCount - 1); i++)
		{
			transforms[i].next = static_cast<uint32_t>(i + 1);
			transforms[i].generation = 1;
		}

		transforms[transformCount - 1].next = UINT32_MAX;
		transforms[transformCount - 1].generation = 1;
	};

	Allocator systemAllocator;

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

TransformHandle TransformPool::CreateTransform(const float3 a_Position)
{
	const uint32_t t_TransformIndex = inst->nextFreeTransform;
	TransformNode* t_Node = &inst->transforms[t_TransformIndex];
	inst->nextFreeTransform = t_Node->next;

	//WILL OVERWRITE t_Node->next due to it being a union.
	new (&t_Node->transform) Transform(a_Position);

	return TransformHandle(t_TransformIndex, t_Node->generation);
}

TransformHandle TransformPool::CreateTransform(const float3 a_Position, const float3 a_Axis, const float a_Radians)
{
	const uint32_t t_TransformIndex = inst->nextFreeTransform;
	TransformNode* t_Node = &inst->transforms[t_TransformIndex];
	inst->nextFreeTransform = t_Node->next;

	//WILL OVERWRITE t_Node->next due to it being a union.
	new (&t_Node->transform) Transform(a_Position, a_Axis, a_Radians);

	return TransformHandle(t_TransformIndex, t_Node->generation);
}

TransformHandle TransformPool::CreateTransform(const float3 a_Position, const float3 a_Axis, const float a_Radians, const float3 a_Scale)
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

const uint32_t TransformPool::PoolSize() const
{
	return inst->transformCount;
}